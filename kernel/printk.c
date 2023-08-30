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

extern int vsprintf(char* buf, const char* fmt, va_list args);

int printk(const char *fmt, ...)
{
    va_list args;               /* va_list is the synoymn of char* */
    int len;

    va_start(args, fmt);
    len = vsprintf(buf, fmt, args); // return the length of buf
    va_end(args);
    __asm__ ("pushw %%fs\n\t"
             "pushw %%ds\n\t"
             "popw  %%fs\n\t" // set %fs = %ds
             "pushl %0\n\t"
             "pushl $buf\n\t"
             "pushl $0\n\t"  // immediate value $0 not %0 :-)
             "call tty_write\n\t"
             "addl $8, %%esp\n\t"
             "popl %0\n\t"
             "popw %%fs\n\t"
             :
             : "r" (len)
            );
    return len;
}

#ifdef DEBUG
int printkc(const char* fmt, ...)
{
    #include <asm/io.h>

    va_list args;
    int len;

    va_start(args, fmt);
    len = vsprintf(buf, fmt, args);
    va_end(args);
    for (int j = 0; j < len; ++j) outb(buf[j], 0xe9);

    return len;
}
#endif

