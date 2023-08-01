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

mm.o: $(asm_objs) $(cc_objs)
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

.PHONY: dep
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
memory.s memory.o: memory.c ../include/signal.h ../include/sys/types.h \
 ../include/asm/system.h ../include/linux/sched.h ../include/linux/head.h \
 ../include/linux/fs.h ../include/linux/mm.h ../include/linux/kernel.h
