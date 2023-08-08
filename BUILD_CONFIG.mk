AS		:= as --32
ASFLAGS	:= #

CC		:= gcc -m32 -march=pentium3 
CFLAGS	:= -O0 -fstrength-reduce -fomit-frame-pointer -finline-functions \
		   -fno-stack-protector -nostdinc -fno-builtin #-Wall 
CPP		:= cpp -nostdinc

LD		:= ld -m elf_i386

AR		:= ar

# Aditional

ASFLAGS	+= -gstabs+
CFLAGS 	+= $(ASFLAGS)
