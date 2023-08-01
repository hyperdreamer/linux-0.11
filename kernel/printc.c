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
#include <asm/io.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

// Print to console of Bochs. If youare are not using Bochs, the function has no effect.
void printc(const char* fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i=vsprintf(buf, fmt, args);
    va_end(args)

    for (int j = 0; j < i; j++) outb(buf[j], 0xe9);
}
