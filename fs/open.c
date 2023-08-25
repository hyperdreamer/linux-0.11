/*
 *  linux/fs/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* #include <string.h> */
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>

extern struct file file_table[NR_FILE];
extern struct tty_struct tty_table[];

int sys_ustat(int dev, struct ustat* ubuf)
{
	return -ENOSYS;
}

int sys_utime(char* filename, struct utimbuf* times)
{
	long actime, modtime;
	struct m_inode* inode = namei(filename);
	if (!inode) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if (times) {
		actime = get_fs_long((laddr_t *) &times->actime);
		modtime = get_fs_long((laddr_t *) &times->modtime);
	} 
    else
		actime = modtime = CURRENT_TIME;
    /***************************************************************/
	inode->i_atime = actime;
	inode->i_mtime = modtime;
    /***************************************************************/
	inode->i_dirt = 1;
	iput(inode);
    /***************************************************************/
	return 0;
}

/*
 * XXX should we use the real or effective uid?  BSD uses the real uid,
 * so as to make this call useful to setuid programs.
 */
int sys_access(const char* filename, int mode)
{
	int res, i_mode;
	struct m_inode* inode = namei(filename);
    /***************************************************************/
	if (!inode) return -EACCES;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	i_mode = res = inode->i_mode & 0777;
	mode &= 0007;
    //////////////////////////////////////////////////////////////////////////
	if (current->uid == inode->i_uid)
		res >>= 6;
	else if (current->gid == inode->i_gid)
		res >>= 6;
    /***************************************************************/
	iput(inode);
    /***************************************************************/
	if ((res & 0007 & mode) == mode) return 0;
	/*
	 * XXX we are doing this test last because we really should be
	 * swapping the effective with the real user id (temporarily),
	 * and then calling suser() routine.  If we do call the
	 * suser() routine, it needs to be called last. 
	 */
	if ((!current->uid) && (!(mode & 1) || (i_mode & 0111))) return 0;
    /***************************************************************/
	return -EACCES;
}

int sys_chdir(const char* filename)
{
	struct m_inode* inode = namei(filename);
    if (!inode) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        return -ENOTDIR;
    }
    //////////////////////////////////////////////////////////////////////////
    iput(current->pwd);
    current->pwd = inode;
    //////////////////////////////////////////////////////////////////////////
    return (0);
}

int sys_chroot(const char* filename)
{
    struct m_inode* inode = namei(filename);
    if (!inode) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}
    //////////////////////////////////////////////////////////////////////////
	iput(current->root);
	current->root = inode;
    //////////////////////////////////////////////////////////////////////////
	return (0);
}

int sys_chmod(const char* filename, int mode)
{
    struct m_inode* inode = namei(filename);
    if (!inode) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if ((current->euid != inode->i_uid) && !suser()) {
		iput(inode);
		return -EACCES;
	}
    /***************************************************************/
	inode->i_mode = (mode & 07777) | (inode->i_mode & ~07777);
	inode->i_dirt = 1;
	iput(inode);
	return 0;
}

// MINIX's gid: unsigned char
int sys_chown(const char* filename, int uid, int gid)
{
    struct m_inode* inode = namei(filename);
    if (!inode) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if (!suser()) {
		iput(inode);
		return -EACCES;
	}
    //////////////////////////////////////////////////////////////////////////
	inode->i_uid = uid;
	inode->i_gid = gid;
	inode->i_dirt = 1;
	iput(inode);
    //////////////////////////////////////////////////////////////////////////
	return 0;
}

int sys_open(const char* filename, int flag, int mode)
{
    int fd;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for(fd = 0; fd < NR_OPEN; ++fd)     // find the first NULL flip
        if (!current->filp[fd]) break;
    /***************************************************************/
    if (fd >= NR_OPEN) return -EINVAL;  // no NULL flip
    /***************************************************************/
    current->close_on_exec &= ~(1 << fd);
    //////////////////////////////////////////////////////////////////////////
    struct file* f = &file_table[0];
    int i;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for (i = 0; i < NR_FILE; ++i, ++f)  // find the first empty slot 
        if (! f->f_count) break;        // in the file_table
    /***************************************************************/
    if (i >= NR_FILE) return -EINVAL;   // no empty slot
    /***************************************************************/
    (current->filp[fd] = f)->f_count++; // searching done!!!
    /*#######################################################################*/
    /*#######################################################################*/
    struct m_inode* inode;
    mode &= 0777 & ~current->umask;
    i = open_namei(filename, flag, mode, &inode);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (i < 0) {    // if open_namei failed, do some clean-up
        current->filp[fd] = NULL;
        f->f_count = 0;
        return i;
    }
    //////////////////////////////////////////////////////////////////////////
    /* ttys are somewhat special (ttyxx major==4, tty major==5) */
    if (S_ISCHR(inode->i_mode)) {
        if (MAJOR(inode->i_zone[0]) == 4) { // ttyxx
            if (current->leader && current->tty < 0) {
                current->tty = MINOR(inode->i_zone[0]);
                tty_table[current->tty].pgrp = current->pgrp;
            }
        } 
        else if (MAJOR(inode->i_zone[0]) == 5) { //tty
            if (current->tty < 0) { // do clean-up
                iput(inode);
                current->filp[fd] = NULL;
                f->f_count=0;
                return -EPERM;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    /* Likewise with block-devices: check for floppy_change */
    if (S_ISBLK(inode->i_mode)) check_disk_change(inode->i_zone[0]);
    /***************************************************************/
    f->f_mode = inode->i_mode;
    f->f_flags = flag;
    f->f_count = 1;
    f->f_inode = inode;
    f->f_pos = 0;
    //////////////////////////////////////////////////////////////////////////
    return fd;
}

int sys_creat(const char* pathname, int mode)
{
	return sys_open(pathname, O_CREAT | O_TRUNC, mode);
}

int sys_close(unsigned int fd)
{	

	if (fd >= NR_OPEN) return -EINVAL;
    /***************************************************************/
	current->close_on_exec &= ~(1 << fd);
    /***************************************************************/
	struct file* filp = current->filp[fd];
	if (!filp) return -EINVAL;  // can't close a NULL filp
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	current->filp[fd] = NULL;
    /***************************************************************/
	if (filp->f_count == 0) panic("Close: file count is 0");
	if (-- filp->f_count) return 0; // if someone else is using it
	iput(filp->f_inode);            // if no one is using it
    /***************************************************************/
	return 0;
}
