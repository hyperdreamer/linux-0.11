#
# Makefile for the FREAX-kernel block device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

CC		:= gcc -m32
CFLAGS	:= -O -Wall -fstrength-reduce -fomit-frame-pointer \
		   -finline-functions -fno-stack-protector -nostdinc -I../../include
AR		:= ar
CPP		:= gcc -m32 -E -nostdinc -I../../include

objs 	= $(patsubst %.c,%.o,$(wildcard *.c))

blk_drv.a: $(objs)
	$(AR) rcs $@ $^
	sync

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

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
floppy.s floppy.o: floppy.c ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/signal.h \
 ../../include/linux/kernel.h ../../include/linux/fdreg.h \
 ../../include/asm/system.h ../../include/asm/io.h \
 ../../include/asm/segment.h blk.h
hd.s hd.o: hd.c ../../include/linux/config.h ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/fs.h \
 ../../include/sys/types.h ../../include/linux/mm.h \
 ../../include/signal.h ../../include/linux/kernel.h \
 ../../include/linux/hdreg.h ../../include/asm/system.h \
 ../../include/asm/io.h ../../include/asm/segment.h blk.h
ll_rw_blk.s ll_rw_blk.o: ll_rw_blk.c ../../include/errno.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/signal.h \
 ../../include/linux/kernel.h ../../include/asm/system.h blk.h
ramdisk.s ramdisk.o: ramdisk.c ../../include/string.h ../../include/linux/config.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/signal.h \
 ../../include/linux/kernel.h ../../include/asm/system.h \
 ../../include/asm/segment.h ../../include/asm/memory.h blk.h
