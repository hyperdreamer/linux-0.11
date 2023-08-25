/*
 *  linux/fs/char_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <sys/types.h>

#include <linux/sched.h>
#include <linux/kernel.h>

#include <asm/segment.h>
#include <asm/io.h>

extern int tty_read(unsigned minor,char* buf, int count);
extern int tty_write(unsigned minor,char* buf, int count);

typedef int (*crw_ptr)(int rw, unsigned minor, char* buf, int count, off_t* pos);

static int rw_ttyx(int rw, unsigned minor, char* buf, int count, off_t* pos)
{
    return ((rw == READ) ? tty_read(minor, buf, count)
                         : tty_write(minor, buf, count));
}

static int rw_tty(int rw, unsigned minor, char* buf, int count, off_t* pos)
{
    if (current->tty < 0) return -EPERM;
    return rw_ttyx(rw, current->tty, buf, count, pos);
}

static int rw_ram(int rw, char* buf, int count, off_t* pos)
{
    return -EIO;
}

static int rw_mem(int rw, char* buf, int count, off_t* pos)
{
    return -EIO;
}

static int rw_kmem(int rw, char* buf, int count, off_t* pos)
{
    return -EIO;
}

static int rw_port(int rw, char* buf, int count, off_t* pos)
{
	int i = *pos;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (count-->0 && i <= 0xffff) {  // 64K
        if (rw == READ)
            put_fs_byte(inb(i), buf++);
        else
            outb(get_fs_byte(buf++), i);
        ++i;
    }
    /***************************************************************/
	i -= *pos;      // the nr of bytes read or written
	*pos += i;      // update the position
    /***************************************************************/
	return i;
}

static int rw_memory(int rw, unsigned minor, char* buf, int count, off_t* pos)
{
    switch(minor) {
    case 0:
        return rw_ram(rw, buf, count, pos);
    case 1:
        return rw_mem(rw, buf, count, pos);
    case 2:
        return rw_kmem(rw, buf, count, pos);
    case 3:
        return (rw == READ) ? 0 : count;	/* rw_null */
    case 4:
        return rw_port(rw, buf, count, pos);
    default:
        return -EIO;
    }
}

static crw_ptr crw_table [] = {
	NULL,		/* nodev */
	rw_memory,	/* /dev/mem etc */
	NULL,		/* /dev/fd */
	NULL,		/* /dev/hd */
	rw_ttyx,	/* /dev/ttyx */
	rw_tty,		/* /dev/tty */
	NULL,		/* /dev/lp */
	NULL		/* unnamed pipes */
};

#define NRDEVS (sizeof(crw_table) / sizeof(crw_ptr))

/*
 **************************** INTERFACE **************************************
 */

int rw_char(int rw, int dev, char* buf, int count, off_t* pos)
{
	if (MAJOR(dev) >= NRDEVS) return -ENODEV;
    /***************************************************************/
	crw_ptr call_addr = crw_table[MAJOR(dev)];
	if (!call_addr) return -ENODEV;
    /***************************************************************/
	return call_addr(rw, MINOR(dev), buf, count, pos);
}

