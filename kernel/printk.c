/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

int printk(const char *fmt, ...)
{
    va_list args;               /* va_list is the synoymn of char* */
    int i;

    va_start(args, fmt);
    i=vsprintf(buf,fmt,args); // return the length of buf
    va_end(args);
    __asm__("pushw %%fs\n\t"
            "pushw %%ds\n\t"
            "popw  %%fs\n\t" // set %fs = %ds
            "pushl %0\n\t"
            "pushl $buf\n\t"
            "pushl $0\n\t"  // immediate value $0 not %0 :-)
            "call tty_write\n\t"   // :tag tty_write, TO_READ
            "addl $8,%%esp\n\t"
            "popl %0\n\t"
            "popw %%fs"
            :
            :"r" (i));
    return i;
}
