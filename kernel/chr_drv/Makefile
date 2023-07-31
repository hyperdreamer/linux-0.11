#
# Makefile for the FREAX-kernel character device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

CC		:= gcc -m32
CFLAGS	:= -O -Wall -fstrength-reduce -fomit-frame-pointer -fvisibility=hidden \
		   -finline-functions -fno-stack-protector -nostdinc -I../../include
AS		:= as --32
AR		:= ar
CPP		:= gcc -m32 -E -nostdinc -I../../include

asm_objs = $(patsubst %.s,%.o,$(wildcard *.s))
cc_objs  = $(patsubst %.c,%.o,$(wildcard *.c))

chr_drv.a: $(cc_objs) $(asm_objs) keyboard.o
	$(AR) rcs $@ $^
	sync

$(cc_objs):%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(asm_objs):%.o: %.s
	$(AS) $< -o $@

keyboard.o: keyboard.S ../../include/linux/config.h
	$(CPP) -traditional $< -o $*.s 
	$(AS) $*.s -o $@
	rm $*.s

.PHONY: clean
clean:
	rm -f core *.o *.a tmp_make
	for i in *.c;do rm -f `basename $$i .c`.s;done

.PHONY: dep
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
console.s console.o: console.c ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/fs.h \
 ../../include/sys/types.h ../../include/linux/mm.h \
 ../../include/signal.h ../../include/linux/tty.h ../../include/termios.h \
 ../../include/asm/io.h ../../include/asm/system.h
serial.s serial.o: serial.c ../../include/linux/tty.h ../../include/termios.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/signal.h \
 ../../include/asm/system.h ../../include/asm/io.h
tty_io.s tty_io.o: tty_io.c ../../include/ctype.h ../../include/errno.h \
 ../../include/signal.h ../../include/sys/types.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/linux/mm.h \
 ../../include/linux/tty.h ../../include/termios.h \
 ../../include/asm/segment.h ../../include/asm/system.h
tty_ioctl.s tty_ioctl.o: tty_ioctl.c ../../include/errno.h ../../include/termios.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/signal.h \
 ../../include/linux/kernel.h ../../include/linux/tty.h \
 ../../include/asm/io.h ../../include/asm/segment.h \
 ../../include/asm/system.h
