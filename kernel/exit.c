/*
 *  linux/kernel/exit.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

extern int sys_pause(void);
extern int sys_close(int fd);   // close file

void release(struct task_struct* p)
{
    if (!p) return;

    for (int i = 1; i < NR_TASKS; ++i)
        if (task[i] == p) {
            task[i] = NULL;
            free_page((unsigned long) p);
            schedule();
            return;
        }

    panic("Trying to release non-existent task!");
}

static inline void fast_release(struct task_struct** p)
{   // careful, by Henry
    free_page((unsigned long) *p);
    *p = NULL;
    schedule();
    return;
}

static inline int send_sig(long sig, struct task_struct* p, int priv)
{
    if (!p || sig<1 || sig>32) return -EINVAL;

    if (priv || (current->euid == p->euid) || suser()) 
    {
        p->signal |= (1<<(sig-1));	// send signal
        return 0;
    }
    
    return -EPERM;
}

static void kill_session(void)
{
    struct task_struct** p = task + NR_TASKS;

    while (--p > &FIRST_TASK) {
        if (*p && (*p)->session == current->session)
            (*p)->signal |= 1<<(SIGHUP-1);
    }
}

/*
 * XXX need to check permissions needed to send signals to process
 * groups, etc. etc.  kill() permissions semantics are tricky!
 */
int sys_kill(int pid, int sig)
{
    struct task_struct** p = task + NR_TASKS;
    int err = 0, retval = 0;

    if (!pid) // pid == 0  
        while (--p > &FIRST_TASK) { // for group members forcefully
            if (*p && (*p)->pgrp == current->pid && (err=send_sig(sig, *p, 1)))
                retval = err;
        } 
    else if (pid > 0) 
        while (--p > &FIRST_TASK) { // for pid
            if (*p && (*p)->pid == pid && (err = send_sig(sig, *p, 0)))
                retval = err;
        } 
    else if (pid == -1) 
        while (--p > &FIRST_TASK) { // for all
            if ((err = send_sig(sig, *p, 0)))
                retval = err;
        }
    else 
        while (--p > &FIRST_TASK) { // for abs(pid)
            if (*p && (*p)->pgrp == -pid && (err = send_sig(sig, *p, 0)))
                retval = err;
        }

    return retval;
}

static void tell_father(int pid)
{
    if (pid)
        for (int i = 0; i < NR_TASKS; ++i) 
        {   // tell father to do some cleanup
            if (!task[i] || task[i]->pid != pid) continue;
            task[i]->signal |= (1<<(SIGCHLD-1));
            return;
        }
    /* if we don't find any fathers, we just release ourselves */
    /* This is not really OK. Must change it to make father 1 */
    printk("BAD BAD - no father found!\n");
    release(current);
}

int do_exit(long code)
{
    free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
    free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
    ////////////////////////////////////////////////////  
    for (int i = 0; i < NR_TASKS; ++i)
        if (task[i] && task[i]->father == current->pid) 
        {
            task[i]->father = 1;
            /* assumption task[1] is always init */
            // tell task[1] to do some cleanup if necessary
            if (task[i]->state == TASK_ZOMBIE)
                (void) send_sig(SIGCHLD, task[1], 1);
        }
    ////////////////////////////////////////////////////  
    for (int i = 0; i < NR_OPEN; ++i)   // close all open files
        if (current->filp[i]) sys_close(i); // TO_READ
    iput(current->pwd); //TO_READ
    current->pwd = NULL;
    iput(current->root);
    current->root = NULL;
    iput(current->executable);
    current->executable = NULL;
    ///////////////////////////////////////////////////
    if (current->leader && current->tty >= 0) tty_table[current->tty].pgrp=0;
    if (last_task_used_math == current) last_task_used_math = NULL;
    if (current->leader) kill_session();
    ///////////////////////////////////////////////////
    current->state = TASK_ZOMBIE;
    current->exit_code = code;
    tell_father(current->father);   // tell its father to do some cleanup
#ifdef DEBUG
    printkc("Current process exits, PID: %d\n", current->pid);
#endif
    schedule();

    return -1;	/* just to suppress warnings */
}

int sys_exit(int error_code)
{
	return do_exit( (error_code & 0xff)<<8 );
}

int sys_waitpid(pid_t pid, unsigned long* stat_addr, int options)
{
    do {
        int flag = 0;
        
        for (struct task_struct** p = &LAST_TASK; p > &FIRST_TASK; --p) {
            if (!*p || *p == current) continue;
            
            if ((*p)->father != current->pid) continue;
            // now p is a child of current
            if (pid > 0) {  // for specific pid
                if ((*p)->pid != pid) continue;
            } 
            else if (!pid) {    // pid ==0: for all group members
                if ((*p)->pgrp != current->pgrp) continue;
            } 
            else if (pid != -1) {   // pid < -1: for specific group
                if ((*p)->pgrp != -pid) continue;
            }
            // pid == -1: for all children
            switch ((*p)->state) {
            case TASK_STOPPED:
                if (!(options & WUNTRACED)) continue;
                verify_area(stat_addr, 4);
                put_fs_long(0x7f, stat_addr);
                return (*p)->pid;
             
            case TASK_ZOMBIE:
                current->cutime += (*p)->utime;
                current->cstime += (*p)->stime;
                flag = (*p)->pid;
                int code = (*p)->exit_code;
                //release(*p);
                fast_release(p); // by Henry
                verify_area(stat_addr, 4);
                put_fs_long(code, stat_addr);
                return flag;
             
            default:
                flag = 1;
                continue;
            }
        }
        
        if (!flag) return -ECHILD;  // No WUNTRACED may trigger this
        
        if (options & WNOHANG) return 0;
        // if no WHNOAHG, always wait for the next hangup
        current->state = TASK_INTERRUPTIBLE;
        schedule();
        // if current recieves non-SIGCHLD
        if ((current->signal &= ~(1<<(SIGCHLD-1))))
            return -EINTR;
    } while (true);
}
