/*
 *  linux/fs/stat.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <sys/stat.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

static void cp_stat(struct m_inode* inode, struct stat* statbuf)
{
    struct stat tmp;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    tmp.st_dev = inode->i_dev;
    tmp.st_ino = inode->i_num;
    tmp.st_mode = inode->i_mode;
    tmp.st_nlink = inode->i_nlinks;
    tmp.st_uid = inode->i_uid;
    tmp.st_gid = inode->i_gid;
    tmp.st_rdev = inode->i_zone[0];
    tmp.st_size = inode->i_size;
    tmp.st_atime = inode->i_atime;
    tmp.st_mtime = inode->i_mtime;
    tmp.st_ctime = inode->i_ctime;
    /***************************************************************/
    copy_to_user(&tmp, statbuf, struct stat);
}

int sys_stat(char* filename, struct stat* statbuf)
{
    struct m_inode* inode = namei(filename);
	if (!inode) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	cp_stat(inode, statbuf);
	iput(inode);
	return 0;
}

int sys_fstat(unsigned int fd, struct stat* statbuf)
{
    struct file* f;
    struct m_inode* inode;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (fd >= NR_OPEN || !(f = current->filp[fd]) || !(inode = f->f_inode))
        return -EBADF;
    /***************************************************************/
    cp_stat(inode,statbuf);
    return 0;
}

// Symbolic links are not implemented in Linux 0.11. So sys_lstat()
// & sys_stat are just the same.
int sys_lstat(char* filename, struct stat* statbuf)
{
    return sys_stat(filename, statbuf);
}

