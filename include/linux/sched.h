#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

#ifndef NULL
#define NULL ((void *) 0)
#endif

/* defined in mm/memory.c */
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);
/* defined in kernel/sched.c */
extern void sched_init(void);
extern void schedule(void);
/* defined in kernel/trap.c */
extern void trap_init(void);
/* defined in kernel/panic.c */
extern void panic(const char * str);
/* defined in kernel/chr_drv/tty_io.c  */
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)(); // for defining a fuction pointer array to hold sys calls

struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};                          /* 108 bytes */

struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero, selector */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;    /* 108 bytes */
};                              /* 104 bytes+108 bytes */

struct task_struct {
/* these are hardcoded - don't touch */
	long state;                 /* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	struct sigaction sigaction[32]; // signal actions for different signals
	long blocked;                   /* bitmap of masked signals */
/* various fields */
	int exit_code;
	unsigned long start_code, end_code, end_data, brk,start_stack;
	long pid, father, pgrp, session, leader;
	unsigned short uid, euid, suid;
	unsigned short gid, egid, sgid;
	long alarm;
	long utime, stime, cutime, cstime, start_time;
	unsigned short used_math;
/* file system info */
	int tty;                    /* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode* pwd;
	struct m_inode* root;
	struct m_inode* executable;
	unsigned long close_on_exec;
	struct file* filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
/*
 * task_struct:
 * 
 * state = 0;
 * counter = 15;
 * priority = 15;
 * 
 * signal = 0;
 * sigaction[32] = {{},}
 * blocked = 0;
 * 
 * exit_code = 0;
 * 
 * start_code = 0;
 * end_code = 0;
 * end_data = 0;
 * brk = 0;
 * start_stack = 0;
 *
 * pid = 0;
 * father = -1;  the first process has no father :-)
 * pgrp = 0;
 * session = 0;
 * leader = 0;
 *
 * uid = 0;
 * euid = 0;
 * suid = 0;
 * gid = 0;
 * egid = 0;
 * sgid = 0;
 *
 * alarm = 0;
 * utime = 0;
 * stime = 0;
 * cutime = 0;
 * cstime = 0;
 * start_time = 0;
 *
 * used_math = 0;
 *
 * tty = -1;  it doen't use any tty
 * umask = 0022;
 * pwd = NULL;
 * root = NULL;
 * executable = NULL;
 * close_on_exec = 0;
 * filp[NR_OPEN] = {NULL,};
 *
 * LDT[1] code segment:
 * 0000, 0000, 1100, 0000, 1111, 1010, 0000, 0000
 * 0000, 0000, 0000, 0000, 0000, 0000, 1001, 1111
 * G=1 (4KB), D=1 (32-bit access mode), U=0 (unused),
 * AVL=0 (inaccessible for os); P=1 (descriptor is valid), DPL=11(bin)
 * S=1, TYPE=1010 (E/A, Unaccessed, nonconforming), BASE=0x0, LIMIT=0x9F
 *
 * LDT[2] data segment:
 * 0000, 0000, 1100, 0000, 1111, 0010, 0000, 0000
 * 0000, 0000, 0000, 0000, 0000, 0000, 1001, 1111
 * G=1 (4KB), D=1 (32-bit access mode), U=0 (unused),
 * AVL=0 (inaccessible for os); P=1 (descriptor is valid), DPL=11(bin)
 * TYPE=0010 (R/W, Unaccessed, up-expanding), BASE=0x0, LIMIT=0x9F
 * 
 */
/*
 * tss:
 * back_link = 0;
 * esp0 = PAGE_SIZE + (long) &init_task;
 * ss0 = 0x10; data segment, dpl = 0, TI = 0 (GDT[2])
 * esp1 = 0;
 * ss1 = 0;		no use for linux kernel dpl=1
 * esp2 = 0;		
 * ss2 = 0;		no use for linux kernel dpl=2
 * cr3 = (long)&pg_dir;
 * 
 * eip = 0;
 * eflags = 0;
 * eax = 0, ecx = 0, edx = 0, ebx = 0;
 * esp = 0;
 * ebp = 0;
 *
 * esi = 0;
 * edi = 0;
 * es = 0x17; data segment, dpl = 3, TI = 1 (LDT[2])
 * cs = 0x0f; code segment, LDT[1] original is 0x17. Changed by Henry
 * ss = 0x17;
 * ds = 0x17;
 * fs = 0x17;
 * gs = 0x17;
 *
 * ldt = _LDT(0); // selector in GDT
 * trace_bitmap = 0x80000000;
 * i387 = {};
 * 
 */ 
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x0f,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}
// original: 0,0,0x17,0x17,0x17,0x17,0x17,0x17,

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;

#define CURRENT_TIME (startup_time+jiffies/HZ)

extern void add_timer(long jiffies, void (*fn)(void));
extern void sleep_on(struct task_struct** p);
extern void interruptible_sleep_on(struct task_struct** p);
extern inline void wake_up(struct task_struct** p);

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)
// _TSS(n), _LDT(n) are both selectors
#define _TSS(n) ((((unsigned long) n)<<4) + (FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4) + (FIRST_LDT_ENTRY<<3))

#define ltr(n) \
    __asm__ ("ltr %%ax\n\t" \
             : \
             : "a" (_TSS(n)) \
            )

#define lldt(n) \
    __asm__ ("lldt %%ax\n\t" \
             : \
             : "a" (_LDT(n)) \
            )

// str(n) gets the task nr of the current task and save it to n
// str instruction copes the current task register to %%ax
// to a 16-bit general register, here %ax
#define str(n) \
    __asm__ ("str %%ax\n\t" \
             "subl %2, %%eax\n\t" \
             "shrl $4, %%eax" \
             : \
             "=a" (n) \
             : \
             "0" (0), \
             "i" (FIRST_TSS_ENTRY<<3) \
            )
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
// __tmp.b holds TSS(n) descriptor, __tmp.a is unimportant
// check ljmp for task switching
#define switch_to(n) \
    static struct {long a,b;} __tmp; \
    __asm__ ("cmpl %3, current\n\t" \
             "je 1f\n\t" \
             "movw %%dx, %1\n\t" \
             "xchgl %3, current\n\t" \
             "ljmp %0\n\t" \
             "cmpl %3, last_task_used_math\n\t" \
             "jne 1f\n\t" \
             "clts\n" \
             "1:\n\t" \
             : \
             : \
             "m" (__tmp.a), \
             "m" (__tmp.b), \
             "d" (_TSS(n)), \
             "r" ((long) task[n]) \
            ); \


#define PAGE_ALIGN(n) ( ((n)+0xfff) & 0xfffff000 )

#define _set_base(addr, base) \
    __asm__ ("movl %3, %%edx\n\t" \
             "movw %%dx, %0\n\t" \
             "rorl $16, %%edx\n\t" \
             "movb %%dl, %1\n\t" \
             "movb %%dh, %2\n\t" \
             : \
             : \
             "m" (addr[2]), \
             "m" (addr[4]), \
             "m" (addr[7]), \
             "r" (base) \
             : \
             "%edx" \
            )

#define _set_limit(addr, limit) \
    __asm__ ("movl %2, %%edx\n\t" \
             "movw %%dx, %0\n\t" \
             "rorl $16, %%edx\n\t" \
             "movb %1, %%dh\n\t" \
             "andb $0xf0, %%dh\n\t" \
             "orb %%dh, %%dl\n\t" \
             "movb %%dl, %1\n\t" \
             : \
             : \
             "m" (addr[0]), \
             "m" (addr[6]), \
             "r" (limit) \
             : \
             "%edx" \
            )

#define set_base(ldt, base) _set_base( ((char*) &(ldt)), base)
#define set_limit(ldt, limit) _set_limit( ((char*) &(ldt)), (limit-1)>>12 )

#define _get_base(addr) \
    ({\
     unsigned long __base; \
     __asm__("movb %3, %%dh\n\t" \
             "movb %2, %%dl\n\t" \
             "shll $16, %%edx\n\t" \
             "movw %1, %%dx\n\t" \
             : \
             "=&d" (__base) \
             : \
             "m" (addr[2]), \
             "m" (addr[4]), \
             "m" (addr[7]) \
            ); \
     __base; \
    })

#define get_base(ldt) _get_base( ((char*) &(ldt)) )

// lsll instruction: loads into the register the
// limit of the descriptor pointed by the selector 
#define get_limit(segment) \
    ({ \
     unsigned long __limit; \
     __asm__("lsll %1, %0\n\t" \
             "incl %0\n\t" \
             : "=&r" (__limit) \
             : "r" (segment) \
            ); \
     __limit; \
    })

#endif
