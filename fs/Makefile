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
AR		:= ar
LD 		:= ld -m elf_i386
LDFLAGS	:= -r
CPP		:= gcc -m32 -march=pentium3 -E -nostdinc -I../include

objs 	= $(patsubst %.c,%.o,$(wildcard *.c))

fs.o: $(objs)
	$(LD) $(LDFLAGS) $^ -o $@
	sync

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

.PHONY: clean
clean:
	rm -f core *.o *.a tmp_make keyboard.s
	for i in *.c;do rm -f `basename $$i .c`.s;done

.PHONY: dep
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
bitmap.s bitmap.o: bitmap.c ../include/string.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h
block_dev.s block_dev.o: block_dev.c ../include/errno.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
 ../include/asm/segment.h ../include/asm/system.h
buffer.s buffer.o: buffer.c ../include/stdarg.h ../include/linux/config.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/sys/types.h ../include/linux/mm.h ../include/signal.h \
 ../include/linux/kernel.h ../include/asm/system.h ../include/asm/io.h
char_dev.s char_dev.o: char_dev.c ../include/errno.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
 ../include/asm/segment.h ../include/asm/io.h
exec.s exec.o: exec.c ../include/errno.h ../include/string.h \
 ../include/sys/stat.h ../include/sys/types.h ../include/a.out.h \
 ../include/linux/fs.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
 ../include/asm/segment.h
fcntl.s fcntl.o: fcntl.c ../include/string.h ../include/errno.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/sys/types.h ../include/linux/mm.h ../include/signal.h \
 ../include/linux/kernel.h ../include/asm/segment.h ../include/fcntl.h \
 ../include/sys/stat.h
file_dev.s file_dev.o: file_dev.c ../include/errno.h ../include/fcntl.h \
 ../include/sys/types.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/linux/mm.h ../include/signal.h \
 ../include/linux/kernel.h ../include/asm/segment.h
file_table.s file_table.o: file_table.c ../include/linux/fs.h ../include/sys/types.h
inode.s inode.o: inode.c ../include/string.h ../include/sys/stat.h \
 ../include/sys/types.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/linux/mm.h ../include/signal.h \
 ../include/linux/kernel.h ../include/asm/system.h
ioctl.s ioctl.o: ioctl.c ../include/string.h ../include/errno.h \
 ../include/sys/stat.h ../include/sys/types.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
 ../include/signal.h
namei.s namei.o: namei.c ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/sys/types.h ../include/linux/mm.h \
 ../include/signal.h ../include/linux/kernel.h ../include/asm/segment.h \
 ../include/string.h ../include/fcntl.h ../include/errno.h \
 ../include/const.h ../include/sys/stat.h
open.s open.o: open.c ../include/string.h ../include/errno.h ../include/fcntl.h \
 ../include/sys/types.h ../include/utime.h ../include/sys/stat.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/tty.h \
 ../include/termios.h ../include/linux/kernel.h ../include/asm/segment.h
pipe.s pipe.o: pipe.c ../include/signal.h ../include/sys/types.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/linux/mm.h ../include/asm/segment.h
read_write.s read_write.o: read_write.c ../include/sys/stat.h ../include/sys/types.h \
 ../include/errno.h ../include/linux/kernel.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
 ../include/signal.h ../include/asm/segment.h
stat.s stat.o: stat.c ../include/errno.h ../include/sys/stat.h \
 ../include/sys/types.h ../include/linux/fs.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/mm.h ../include/signal.h \
 ../include/linux/kernel.h ../include/asm/segment.h
super.s super.o: super.c ../include/linux/config.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/signal.h ../include/linux/kernel.h \
 ../include/asm/system.h ../include/errno.h ../include/sys/stat.h
truncate.s truncate.o: truncate.c ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/sys/types.h ../include/linux/mm.h \
 ../include/signal.h ../include/sys/stat.h
