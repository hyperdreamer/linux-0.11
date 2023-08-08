#
# Makefile for the FREAX-kernel block device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
include ../../BUILD_CONFIG.mk

CFLAGS	+= -I../../include
CPP		+= $(CFLAGS)

objs 	= $(patsubst %.c,%.o,$(wildcard *.c))

math.a: $(objs)
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
math_emulate.s math_emulate.o: math_emulate.c ../../include/signal.h \
 ../../include/sys/types.h ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/fs.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/asm/segment.h
