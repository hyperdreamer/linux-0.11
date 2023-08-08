AS		:= as --32
ASFLAGS	:= #

CC		:= gcc -m32 -march=pentium3 
CFLAGS	:= -O -Wall -fstrength-reduce -fomit-frame-pointer -finline-functions \
		   -fno-stack-protector -nostdinc
CPP		:= $(CC) -E -nostdinc

LD		:= ld -m elf_i386

AR		:= ar

# Aditional

#CFLAGS 	+= -g
