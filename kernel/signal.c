/*
 *  linux/kernel/signal.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>

extern volatile int do_exit(int error_code);

int sys_sgetmask()
{
	return current->blocked;
}

int sys_ssetmask(int newmask)
{
	int old = current->blocked;

	current->blocked = newmask & ~(1<<(SIGKILL-1));
	return old;
}

/* "static" means it can only be accessed in the current file */
/*
static inline void save_old(char* from, char* to)
{
	verify_area(to, sizeof(struct sigaction));
	for (int i = 0; i < sizeof(struct sigaction); i++) {
		put_fs_byte(*from,to);
		from++;
		to++;
	}
}
*/

/*
static inline void get_new(char* from, char* to)
{
    for (int i = 0; i < sizeof(struct sigaction); i++)
        *(to++) = get_fs_byte(from++);
}
*/

int sys_signal(int signum, long handler, long restorer)
{
	if (signum<1 || signum>32 || signum==SIGKILL) return -1;
    //////////////////////////////////////////////////////////////////////////
	struct sigaction tmp;
	tmp.sa_handler = (void (*)(int)) handler;
	tmp.sa_mask = 0;
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
	tmp.sa_restorer = (void (*)(void)) restorer;
    /***************************************************************/
	handler = (long) current->sigaction[signum-1].sa_handler;
	current->sigaction[signum-1] = tmp;
    //////////////////////////////////////////////////////////////////////////
	return handler; // return the old handler
}

int sys_sigaction(int signum, 
                  const struct sigaction* action,
                  struct sigaction* oldaction)
{
	if (signum < 1 || signum > 32 || signum == SIGKILL) return -1;
    //////////////////////////////////////////////////////////////////////////
	struct sigaction tmp = current->sigaction[signum-1];
    /***************************************************************/
    copy_block_fs2es((const char*) action,
                     (char*) (signum-1 + current->sigaction),
                     sizeof(struct sigaction));
    /***************************************************************/
	if (oldaction) copy_to_user(&tmp, oldaction, struct sigaction);
    //////////////////////////////////////////////////////////////////////////
	if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
		current->sigaction[signum-1].sa_mask = 0;
	else
		current->sigaction[signum-1].sa_mask |= (1<<(signum-1));

	return 0;
}

void do_signal(long signr, long eax, long ebx, long ecx, long edx,
               long fs, long es, long ds, long eip, long cs, long eflags,
               unsigned long* esp, long ss)
{
    unsigned long sa_handler;
    long old_eip = eip;
    struct sigaction* sa = current->sigaction + signr-1;	// get signaction
    int longs;
    unsigned long* tmp_esp;

    sa_handler = (unsigned long) sa->sa_handler;
    if (sa_handler == 1) return;    // SIG_IGN

    if (!sa_handler) {  // SIG_DFL
        if (signr == SIGCHLD) return;
        /********************************************************/
        do_exit(1<<(signr-1));
    }

    if (sa->sa_flags & SA_ONESHOT) sa->sa_handler = NULL;

    *(&eip) = sa_handler;	// signal process handler
    longs = (sa->sa_flags & SA_NOMASK) ? 7 : 8;
    *(&esp) -= longs;
    verify_area(esp, longs*4);	// copy on write
    tmp_esp = esp;
    put_fs_long((long) sa->sa_restorer, tmp_esp++);	// push restorer into user stack
    put_fs_long(signr, tmp_esp++);		// push signal value
    if (!(sa->sa_flags & SA_NOMASK)) 	// if none mask then push block signal value
        put_fs_long(current->blocked, tmp_esp++);
    put_fs_long(eax, tmp_esp++);			// push registers
    put_fs_long(ecx, tmp_esp++);
    put_fs_long(edx, tmp_esp++);
    put_fs_long(eflags, tmp_esp++);
    put_fs_long(old_eip, tmp_esp++);
    current->blocked |= sa->sa_mask;	// mask current processed signal
}

int sys_sigpending(sigset_t *set)
{
    /* fill in "set" with signals pending but blocked. */
    verify_area(set,4);
    put_fs_long(current->blocked & current->signal, (unsigned long *)set);
    return 0;
}

/* atomically swap in the new signal mask, and wait for a signal.
 *
 * we need to play some games with syscall restarting.  We get help
 * from the syscall library interface.  Note that we need to coordinate
 * the calling convention with the libc routine.
 *
 * "set" is just the sigmask as described in 1003.1-1988, 3.3.7.
 * 	It is assumed that sigset_t can be passed as a 32 bit quantity.
 *
 * "restart" holds a restart indication.  If it's non-zero, then we 
 * 	install the old mask, and return normally.  If it's zero, we store 
 * 	the current mask in old_mask and block until a signal comes in.
 */
int sys_sigsuspend(int restart, unsigned long old_mask, unsigned long set)
{
    return -ENOSYS;
}
