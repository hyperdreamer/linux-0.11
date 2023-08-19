/*
 *  linux/kernel/sys.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/types.h>

int sys_ftime()
{
	return -ENOSYS;
}

int sys_break()
{
	return -ENOSYS;
}

int sys_ptrace()
{
	return -ENOSYS;
}

int sys_stty()
{
	return -ENOSYS;
}

int sys_gtty()
{
	return -ENOSYS;
}

int sys_rename()
{
	return -ENOSYS;
}

int sys_prof()
{
	return -ENOSYS;
}

int sys_setregid(int rgid, int egid)
{
    if (rgid > 0) {
        if ((current->gid == rgid) || suser())
            current->gid = rgid;
        else
            return -EPERM;
    }

    if (egid > 0) {
        if ((current->gid == egid) ||
            (current->egid == egid) ||
            (current->sgid == egid) ||
            suser())
            current->egid = egid;
        else
            return -EPERM;
    }
    return 0;
}

int sys_setgid(int gid)
{
	return(sys_setregid(gid, gid));
}

int sys_acct()
{
	return -ENOSYS;
}

int sys_phys()
{
	return -ENOSYS;
}

int sys_lock()
{
	return -ENOSYS;
}

int sys_mpx()
{
	return -ENOSYS;
}

int sys_ulimit()
{
	return -ENOSYS;
}

int sys_time(time_t* tloc)
{
    int i = CURRENT_TIME;
    
    if (tloc) copy_long_to_user(i, tloc);
    
	return i;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa.
 */
int sys_setreuid(int ruid, int euid)
{
    int old_ruid = current->uid;

    if (ruid > 0) {
        if ((current->euid == ruid) ||
            (old_ruid == ruid) ||
            suser())
            current->uid = ruid;
        else
            return -EPERM;
    }

    if (euid > 0) {
        if ((old_ruid == euid) ||
            (current->euid == euid) ||
            suser())
            current->euid = euid;
        else {
            current->uid = old_ruid;
            return -EPERM;
        }
    }
    return 0;
}

int sys_setuid(int uid)
{
	return(sys_setreuid(uid, uid));
}

int sys_stime(time_t* tptr)
{
	if (!suser()) return -EPERM;

	startup_time = get_fs_long((unsigned long*) tptr) - jiffies/HZ;
	return 0;
}

int sys_times(struct tms* tbuf)
{
    if (tbuf) copy_to_user(&(current->utime), tbuf, struct tms);

    return jiffies;
}

int sys_brk(unsigned long end_data_seg) // I don't get it :-(
{
    if (end_data_seg >= current->end_code &&
        end_data_seg < current->start_stack - 16384)
        current->brk = end_data_seg;
    return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
int sys_setpgid(int pid, int pgid)
{
    if (!pid) pid = current->pid;
    if (!pgid) pgid = current->pid;

    for (register int i = 0; i < NR_TASKS; ++i)
        if (task[i] && task[i]->pid == pid) {
            if (task[i]->leader) return -EPERM;
            if (task[i]->session != current->session) return -EPERM;
            
            task[i]->pgrp = pgid;
            return 0;
        }

    return -ESRCH;
}

int sys_getpgrp(void)
{
	return current->pgrp;
}

int sys_setsid(void)
{
	if (current->leader && !suser())
		return -EPERM;

	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;

	return current->pgrp;
}

int sys_uname(struct utsname* utsbuf)
{
    static struct utsname thisname = {
        "linux .0", "nodename", "release ", "version ", "machine "
    };

	if (!utsbuf) return -ERROR;

    copy_to_user(&thisname, utsbuf, struct utsname);
	return 0;
}

int sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return old;
}

int sys_sethostname(char *name, int len)
{
    return -ENOSYS;
}

int sys_getrlimit()
{
    return -ENOSYS;
}

int sys_setrlimit()
{
    return -ENOSYS;
}

/*
 * It would make sense to put struct rusuage in the task_struct,
 * except that would make the task_struct be *really big*.  After
 * task_struct gets moved into malloc'ed memory, it would
 * make sense to do this.  It will make moving the rest of the information
 * a lot simpler!  (Which we're not doing right now because we're not
 * measuring them yet).
 */
int sys_getrusage()
{
    return -ENOSYS;
}

int sys_gettimeofday()
{
    return -ENOSYS;
}

/*
 * The first time we set the timezone, we will warp the clock so that
 * it is ticking GMT time instead of local time.  Presumably, 
 * if someone is setting the timezone then we are running in an
 * environment where the programs understand about timezones.
 * This should be done at boot time in the /etc/rc script, as
 * soon as possible, so that the clock can be set right.  Otherwise,
 * various programs will get confused when the clock gets warped.
 */
int sys_settimeofday()
{
    return -ENOSYS;
}

int sys_getgroups(int gidsetsize, gid_t *grouplist)
{
    return -ENOSYS;
}

int sys_setgroups(int gidsetsize, gid_t *grouplist)
{
    return -ENOSYS;
}

int sys_select()
{
    return -ENOSYS;
}

int sys_uselib()
{
    return -ENOSYS;
}

int do_bad_syscall(int syscall_nr)
{
#ifdef DEBUG
    printk("Bad syscall nr: %d\n", syscall_nr);
    printkc("Bad syscall nr: %d\n", syscall_nr);
#endif
    return -EPERM;
}

