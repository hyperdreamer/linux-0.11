#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
RAMDISK 	:= #-DRAMDISK=512

AS			:= as --32
LD			:= ld -m elf_i386
LDFLAGS		:= -s -x -M			# for optimization
#LDFLAGS		:= -M				# for debug
LDFILE 		:= kernel.ld
LDFILE_BOOT := boot/boot.ld
CC			:= gcc -m32 $(RAMDISK)
CFLAGS		:= -O -Wall -fstrength-reduce -fomit-frame-pointer \
			   -finline-functions -fno-stack-protector -nostdinc -Iinclude
CPP			:= gcc -m32 -E -nostdinc -Iinclude
OBJCOPY 	:= objcopy -R .pdr -R .comment -R .note -S -O binary
CTAGS		:= /usr/bin/ctags --c-kinds=+p --c++-kinds=+p --fields=+iaS --extra=+qf -R

#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, /dev/xxxx or empty, in which case the
# default of /dev/hd6 is used by 'build'.
#
archives 	:= kernel/kernel.o mm/mm.o fs/fs.o
drivers  	:= kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
math	 	:= kernel/math/math.a
libs	 	:= lib/lib.a
subs  	 	:= $(archives) $(drivers) $(math) $(libs)
components 	:= boot/head.o init/main.o $(subs)
subdirs	 	:= $(dir $(subs))

.PHONY: all
all: sub_make tags 

.PHONY: sub_make
sub_make: $(subdirs)
	@$(foreach prereq,$^,make -C $(prereq);)

tags: boot.img
	-$(CTAGS)
	sync

boot.img: boot/bootsect boot/setup tools/system 
	cp -f tools/system system.tmp
	strip system.tmp
	$(OBJCOPY) system.tmp tools/kernel
	rm system.tmp
	dd if=boot/bootsect of=$@ bs=512 count=1
	dd if=boot/setup of=$@ seek=1 bs=512 count=4
	dd if=tools/kernel of=$@ seek=5
	rm tools/kernel
	sync

boot/head.o: boot/head.s
	$(AS) $< -o $@

int/main.o: init/main.c
	$(CC) $(CFLAGS) -c $< -o $@ 

tools/system: $(components) $(LDFILE)
	$(LD) $(LDFLAGS) $(components) -o $@ -T $(LDFILE) > System.map

boot/setup: boot/setup.s $(LDFILE_BOOT)
	$(AS) $< -o boot/setup.o
	$(LD) boot/setup.o -o boot/setup.elf -T $(LDFILE_BOOT)
	$(OBJCOPY) boot/setup.elf $@
	rm boot/setup.o boot/setup.elf

boot/bootsect: boot/bootsect.s $(LDFILE_BOOT)
	$(AS) $< -o boot/bootsect.o 
	$(LD) boot/bootsect.o -o boot/bootsect.elf -T $(LDFILE_BOOT)
	$(OBJCOPY) boot/bootsect.elf $@
	rm boot/bootsect.o boot/bootsect.elf

.PHONY: clean
clean: $(subdirs)
	rm -f boot.img System.map tmp_make core boot/bootsect boot/setup
	rm -f init/*.o tools/system boot/*.o
	@$(foreach prereq,$^,make clean -C $(prereq);)
	
.PHONY: backup
backup: clean
	(cd .. ; tar cf - linux | compress - > backup.Z)
	sync

.PHONY: dep
dep: $(subdirs)
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	-@$(foreach prereq,$^,make dep -C $(prereq);)

### Dependencies:
init/main.o: init/main.c include/unistd.h include/sys/stat.h \
 include/sys/types.h include/sys/times.h include/sys/utsname.h \
 include/utime.h include/time.h include/linux/tty.h include/termios.h \
 include/linux/sched.h include/linux/head.h include/linux/fs.h \
 include/linux/mm.h include/signal.h include/asm/system.h \
 include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h
