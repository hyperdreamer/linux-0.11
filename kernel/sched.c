/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _S(nr) (1<<((nr)-1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i]) /* very smart: (p+1) :o */
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void)
{
	for (int i=0; i < NR_TASKS; i++)
		if (task[i])
			show_task(i,task[i]);
}

// LATCH := 1193180 / 100: the timer frequency is 100 Hz
#define LATCH (1193180/HZ)

/* extern void mem_use(void); */

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];      /* one page size */
} __attribute__((aligned(PAGE_SIZE)));

static union task_union init_task = {INIT_TASK,};

long volatile jiffies = 0;
long startup_time = 0;
struct task_struct* current = &(init_task.task);
struct task_struct* last_task_used_math = NULL;

struct task_struct* task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;      /* PAGE_SIZE == 4096 */
                                        /* PAGE_SIZE >> 2 == 1024 */
                                        /* user_stack is 4KB, a page size */

struct {                /* head.s uses this */
	long * a;           /* offset for %esp */
	short b;            /* ss */
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x2<<3 }; /* GTD[2] */

/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	static struct task_struct** p;
    static int next, c, i;
/* check alarm, wake up any interruptible tasks that have got a signal */
    for (p = &LAST_TASK; p > &FIRST_TASK; --p) /* TO READ */
        if (*p) {
            if ((*p)->alarm && (*p)->alarm < jiffies) 
            {
                (*p)->signal |= (1<<(SIGALRM-1));
                (*p)->alarm = 0;
            }
            if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) 
                && (*p)->state == TASK_INTERRUPTIBLE)
            {
                (*p)->state = TASK_RUNNING;
            }
        }
/* this is the scheduler proper: */
    do {
        c = -1;                 /* why not 0, can counter be < -1 ? */
        next = 0;
        i = NR_TASKS;
        p = task + NR_TASKS;    // the ghost task

        while (--i) {   // i in [NR_TASKS..1], task[0] will never be touched
            if (!*--p) continue;
            // get the task with the largest counter :-)
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c) 
                c = (*p)->counter, next = i;
        }
        if (c) break; // if the largest counter is larger than 0 then break
                      // otherwise reset counters of all tasks  
        for(p = &LAST_TASK; p > &FIRST_TASK; --p) 
            if (*p) (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
    } while (1);

	switch_to(next);
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state = TASK_RUNNING;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	unsigned char mask = 0x10;
	for (int i=0; i<4; i++, mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
    if (!fn) return;

    cli();
    if (jiffies <= 0) (fn)();
    else {
        struct timer_list* p;
        for (p = timer_list; p < timer_list + TIME_REQUESTS; ++p)
            if (!p->fn) break;

        if (p >= timer_list + TIME_REQUESTS)
            panic("No more time requests free");

        p->fn = fn;
        p->jiffies = jiffies;
        p->next = next_timer;
        if (next_timer->jiffies >= jiffies) next_timer = p;
        struct timer_list* tmp;
        while ((tmp = p->next) && tmp->jiffies < p->jiffies) {
            p->next = tmp->next;
            tmp->next = p;
            p->jiffies -= tmp->jiffies;
        }
        if (tmp && tmp->jiffies >= p->jiffies) 
            tmp->jiffies -= p->jiffies; 
        
    }
    sti();
}

void do_timer(long cpl)
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount && !--beepcount) sysbeepstop();

    cpl ? current->utime++ : current->stime++;

    if (next_timer) {
        next_timer->jiffies--;
        while (next_timer && next_timer->jiffies <= 0) {
            void (*fn)(void);
            fn = next_timer->fn;
            next_timer->fn = NULL;
            next_timer = next_timer->next;
            (fn)();
        }
    }
	if (current_DOR & 0xf0) do_floppy_timer();
	if (--current->counter > 0) return; // process still has time, no
                                        // scheduling
	current->counter = 0;   // it is important, a process with counter 0
                            // can do_timer() and get -1
	if (!cpl) return;       // kernel mode, no scheduling

	schedule();
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
    /* set tss0 and ldt0; here the type of gdt is "desc_struct" */
	set_tss_desc(0, &(init_task.task.tss));
	set_ldt_desc(0, &(init_task.task.ldt));

	struct desc_struct* p;
	p = gdt + FIRST_TSS_ENTRY + 2;       /* tss1 */
	for (int i=1; i < NR_TASKS; i++) {  /* initite (tss1, ldt1)--(tss63, ldt63) */
		task[i] = NULL;
		p->a = p->b = 0; // tssi
		p++;
		p->a = p->b = 0; // ldti
		p++;
	}
    /* Clear NT(nested task), so that we won't have troubles with that later on */
    // pay attention to the skill to modify the eflags :-)
	__asm__ ("pushfl\n\t"
             "andl $0xffffbfff, (%esp)\n\t" 
             "popfl\n\t");
	ltr(0);  // load task register for task0
	lldt(0); /* load ldt for task0  */

    // initiate i8253, the timer_interrupt
    // 0x36 == 00,11,011,0 
    // 0-bit == 0: 16-bit binary
    // 1~3-bit == 011: square wave generator
    // 4~5-bit == 11: lobyte first, then hibyte
    // 6~7-bit == 00: channel 0: IRQ_0, the timer: send data to 0x40
    // Check: https://wiki.osdev.org/Programmable_Interval_Timer
    outb_p(0x36, 0x43);                     /* binary, mode 3, LSB/MSB, counter 0 */
    // Send LATCH to 0x40: the timer frequency 100 Hz
	outb_p(LATCH & 0xff, 0x40);             /* LSB */
	outb(LATCH >> 8, 0x40);                 /* MSB */
    // set timer interrupt handler and enable IRQ_0
	set_intr_gate(0x20, &timer_interrupt);
	outb(inb_p(0x21) & ~0x01, 0x21);        /* enable IRQ_0 */

	set_system_gate(0x80, &system_call);
}
 
