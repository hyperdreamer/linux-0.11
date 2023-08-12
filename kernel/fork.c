/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>

#include <linux/sched.h>
#include <asm/segment.h>
#include <asm/system.h>

extern int printk(const char* fmt, ...);
extern void write_verify(unsigned long address);

static long last_pid = 0;     // "static" added by Henry

void verify_area(void* addr, int size)
{
	laddr_t start;

	start = (laddr_t) addr;
	size += start & 0xfff;	
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size > 0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr, struct task_struct* p)
{
	unsigned long old_data_base, new_data_base, data_limit;
	unsigned long old_code_base, new_code_base, code_limit;

	code_limit = get_limit(0x0f);  	// ldt[1]
    data_limit = get_limit(0x17);  	// ldt[2]
    old_code_base = get_base(current->ldt[1]); 	// no instruction loads base
	old_data_base = get_base(current->ldt[2]); 	// directly

	if (old_data_base != old_code_base) 		// ensure code and data segment 
		panic("We don't support separate I&D"); // overlap
	if (data_limit < code_limit)	            // obvious data > code
		panic("Bad data_limit");

	new_data_base = new_code_base = nr * 0x4000000;	 // nr * 64MB 
	p->start_code = new_code_base;

    set_base(p->ldt[1], new_code_base);
    set_base(p->ldt[2], new_data_base);
    if (copy_page_tables(old_data_base, new_data_base, data_limit)) 
    {
        printk("free_page_tables: from copy_mem\n");
        free_page_tables(new_data_base, data_limit);
        return -ENOMEM;
    }
    return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
                 long ebx,long ecx,long edx, long fs, long es, long ds,
                 long eip, long cs, long eflags, long esp, long ss)
{
	struct task_struct* p = (struct task_struct *) get_free_page();
	if (!p) return -EAGAIN;

	task[nr] = p;
    //* NOTE! this doesn't copy the supervisor stack */
	//*p = *current;
    copy_block((const char*) current, (char*) p, sizeof(struct task_struct));

    p->state = TASK_UNINTERRUPTIBLE;
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
	p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long) p;
	p->tss.ss0 = 0x10;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;		// that's why new forked process return pid 0 :-)
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;   // what's that for?

	if (last_task_used_math == current)
		__asm__ ("clts\n\t"
                 "fnsave %0\n\t"
                 :
                 :"m" (p->tss.i387)
                );

	if (copy_mem(nr, p)) {
		task[nr] = NULL;	    // fork fail :-(
		free_page((long) p);    // then free task struct
		return -EAGAIN;
	}
    /////////////////////////////////////////////////////  
	for (register int i = 0; i < NR_OPEN; ++i) {
        struct file* f = p->filp[i];
        if (f) ++(f->f_count);
    }

	if (current->pwd) ++(current->pwd->i_count);
	if (current->root) ++(current->root->i_count);
	if (current->executable) ++(current->executable->i_count);
    /////////////////////////////////////////////////////
	set_tss_desc(nr, &(p->tss));
	set_ldt_desc(nr, &(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case */

	return last_pid;	        // return pid of new forked process
}

int find_empty_process(void)
{
	register int i;
    /////////////////////////////////////////////////////////////////
    // find an unused id for new process
    // since ++last_pid can make last_pid overflow, it's neccessary to enusre
    // (++last_pid)>0.
repeat:
    if ((++last_pid) < 0) last_pid = 1; 
    for (i = 0; i < NR_TASKS; ++i)
        if (task[i] && task[i]->pid == last_pid) goto repeat;
    ////////////////////////////////////////////////////////////////        
    for (i = 1; i < NR_TASKS; ++i)  /* find an empty slot for new process */
        if (!task[i]) return i;
    return -EAGAIN;
}
