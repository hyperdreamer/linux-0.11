#
# Makefile for the FREAX-kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
CC		:= gcc -m32 -march=pentium3
CFLAGS	:= -O -Wall -fstrength-reduce -fomit-frame-pointer \
		   -finline-functions -fno-stack-protector -nostdinc -I../include
AS		:= as --32
AR		:= ar
LD 		:= ld -m elf_i386
LDFLAGS	:= -r
CPP		:= gcc -m32 -march=pentium3 -E -nostdinc -I../include

asm_objs = $(patsubst %.s,%.o,$(wildcard *.s))
cc_objs  = $(patsubst %.c,%.o,$(wildcard *.c))

kernel.o: $(asm_objs) $(cc_objs)
	$(LD) $(LDFLAGS) $^ -o $@
	sync

$(cc_objs):%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(asm_objs):%.o: %.s
	$(AS) $< -o $@

.PHONY: clean
clean:
	rm -f core *.o *.a tmp_make keyboard.s
	for i in *.c;do rm -f `basename $$i .c`.s;done
	(cd chr_drv; make clean)
	(cd blk_drv; make clean)
	(cd math; make clean)

.PHONY: dep
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd chr_drv; make dep)
	(cd blk_drv; make dep)

### Dependencies:
exit.s exit.o: exit.c ../include/errno.h ../include/signal.h \
 ../include/sys/types.h ../include/sys/wait.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
 ../include/linux/kernel.h ../include/linux/tty.h ../include/termios.h \
 ../include/asm/segment.h
fork.s fork.o: fork.c ../include/errno.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
 ../include/asm/segment.h ../include/asm/system.h
mktime.s mktime.o: mktime.c ../include/time.h
panic.s panic.o: panic.c ../include/linux/kernel.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h
printc.s printc.o: printc.c ../include/stdarg.h ../include/stddef.h \
 ../include/linux/kernel.h ../include/asm/io.h
printk.s printk.o: printk.c ../include/stdarg.h ../include/stddef.h \
 ../include/linux/kernel.h
sched.s sched.o: sched.c ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/sys/types.h ../include/linux/mm.h \
 ../include/signal.h ../include/linux/kernel.h ../include/linux/sys.h \
 ../include/linux/fdreg.h ../include/asm/system.h ../include/asm/io.h \
 ../include/asm/segment.h
signal.s signal.o: signal.c ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/sys/types.h ../include/linux/mm.h \
 ../include/signal.h ../include/linux/kernel.h ../include/asm/segment.h
sys.s sys.o: sys.c ../include/errno.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/tty.h \
 ../include/termios.h ../include/linux/kernel.h ../include/asm/segment.h \
 ../include/sys/times.h ../include/sys/utsname.h
traps.s traps.o: traps.c ../include/string.h ../include/linux/head.h \
 ../include/linux/sched.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
 ../include/asm/system.h ../include/asm/segment.h ../include/asm/io.h
vsprintf.s vsprintf.o: vsprintf.c ../include/stdarg.h ../include/string.h
