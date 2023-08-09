#
#       setup.s         (C) 1991 Linus Torvalds
#                       (C) 2009 He Wen
#
# setup.s is responsible for getting the system data from the BIOS,
# and putting them into the appropriate places in system memory.
# both setup.s and system has been loaded by the bootblock.
#
# This code asks the bios for memory/disk/other parameters, and
# puts them in a "safe" place: 0x90000-0x901FF, ie where the
# boot-block used to be. It is then up to the protected mode
# system to read them from there before the area is overwritten
# for buffer-blocks.
#

# NOTE! These had better be the same as in bootsect.s!

.code16
.section .text

.equ INITSEG,  0x9000   # we move boot here - out of the way
.equ SYSSEG,   0x1000   # system loaded at 0x10000 (65536).
.equ SETUPSEG, 0x9020   # this is the current segment 

.global _start
_start:
    
# ok, the read went well so we get current cursor position and save it for
# posterity. (0x90000)
    
    movw $INITSEG, %ax  # this is done in bootsect already, but...
    movw %ax, %ds
    movb $0x03, %ah     # read cursor pos
    xorb %bh, %bh       # BH (video page) should be 0
    int  $0x10          # save it in a known place, con_init fetches
    movw %dx, 0         # it from 0x90000. DH: row; DL:column
    
# Get memory size (extended mem, kB) (0x90002)

    movb $0x88, %ah     # %ax nr of contiguous 1KB blocks of memory starting
    int  $0x15          # at address 1024KB. Remember not at 0
    movw %ax, 2

# Get video-card data: (0x90004, 0x90006)

    movb $0x0f, %ah
    int  $0x10
    movw %bx, 4         # bh = current display page
    movw %ax, 6         # al = video mode, ah = window width (nr of screen columns)

# check for EGA/VGA and some config parameters (0x90008, 0x9000a, 0x9000c)

    movb $0x12, %ah
    movb $0x10, %bl
    int  $0x10                  # Maybe just a bug here, since the interruption
    movw %ax, 8                 # doesn't returen anything on %ax
    movw %bx, 10                # BH: color or mono; BL = EGA memory size
    movw %cx, 12                # CH = feature bits; CL = switch settingsw

# Get hd0 data to 0x90080, 16bytes

    movw $0x0000, %ax           # interrup vector 0x41, with address 0x41*4
    movw %ax, %ds               # it stores the address of the info table of hd0
    lds  0x41*4, %si            # load %si with the value stored at address 0x41*4
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0080, %di
    movw $0x0008, %cx
    cld
    rep  movsw

# Get hd1 data
    
    movw $0x0000, %ax
    movw %ax, %ds
    lds  4*0x46, %si
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0090, %di
    movw $0x0008, %cx
    cld
    rep  movsw

# Check whether there is a hd1 :-)
    movb $0x15, %ah
    movb $0x81, %dl
    int  $0x13
    jc   no_disk1
    cmpb $0x03, %ah
    je   is_disk1
no_disk1:                       # if no_disk1, set 0x90090-0x9009F zero
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0090, %di
    xorw %ax, %ax
    movw $0x0008, %cx
    cld
    rep  stosw
is_disk1:

# now we wanna move to protected mode ...

    cli                 # no interrupts allowed! (set IF in EFLAGS 0)

# first we movw the system to its rightful place
    movw $0x0000, %ax
    cld
do_move:                # $0x10000->$0x00000,...,$0x80000->$0x70000
    movw %ax, %es       # destination segment
    addw $0x1000, %ax
    cmpw $0x9000, %ax
    je   end_move
    movw %ax, %ds       # source segment
    xorw %si, %si
    xorw %di, %di
    movw $0x8000, %cx   # very smart :-)
    rep  movsw
    jmp  do_move

# then we load the segment descriptors

end_move:
    movw $SETUPSEG, %ax # right, forget this at first. didn't work :-)
    movw %ax, %ds       # lgdt, lidt, use %ds:label
    lidt idt_48         # load idt with 0, 0
    lgdt gdt_48         # load gdt with whatever appropriate

# that was painless, now we enable A20
    call empty_8042
    movb $0xD1, %al     # write command: next byte written to port 60h is placed in
    out  %al, $0x64     # the 8042 output port (which is inaccessible to the data bus) 
    call empty_8042
    movb $0xDF, %al
    out  %al, $0x60     # A20 on
    call empty_8042

# well, that went ok, I hope. Now we have to reprogram the interrupts :-(
# we put them right after the intel-reserved hardware interrupts, at
# int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
# messed this up with the original PC, and they haven't been able to
# rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
# which is used for the internal hardware interrupts as well. We just
# have to reprogram the 8259's, and it isn't fun.

    # ICW_1 0,0001,0001
    movb    $0x11, %al              # initialization sequence
    out     %al, $0x20              # send it to 8259A-1
    .word   0x00eb, 0x00eb          # jmp $+2, jmp $+2, make a little delay
    out     %al, $0xA0              # and to 8259A-2
    .word   0x00eb, 0x00eb

    # ICW_2 
    movb    $0x20, %al              # start of hardware int's (0x20)
    out     %al, $0x21
    .word   0x00eb, 0x00eb
    movb    $0x28, %al              # start of hardware int's 2 (0x28)
    out     %al, $0xA1
    .word   0x00eb, 0x00eb

    # ICW_3
    movb    $0x04, %al              # 8259-1 is master
    out     %al, $0x21
    .word   0x00eb, 0x00eb
    movb    $0x02, %al              # 8259-2 is slave
    out     %al, $0xA1
    .word   0x00eb, 0x00eb

    # ICW_4
    movb    $0x01, %al              # 8086 mode for both
    out     %al, $0x21
    .word   0x00eb, 0x00eb
    out     %al, $0xA1
    .word   0x00eb, 0x00eb

    # OCW_1
    movb    $0xFF, %al              # mask off all interrupts for now
    out     %al, $0x21
    .word   0x00eb, 0x00eb
    out     %al, $0xA1

# well, that certainly wasn't fun :-(. Hopefully it works, and we don't
# need no steenking BIOS anyway (except for the initial loading :-).
# The BIOS-routine wants lots of unnecessary data, and it's less
# "interesting" anyway. This is how REAL programmers do it.
#
# Well, now's the time to actually move into protected mode. To make
# things as simple as possible, we do no register set-up or anything,
# we let the gnu-compiled 32-bit programs do that. We just jump to
# absolute address 0x00000, in 32-bit protected mode.

    movw $0x0001, %ax       # set PE bit in CR0 to switch to protection mode
    lmsw %ax                # This is it!   (logical or)
    ljmp $1<<3, $0x0000     # GDT[1]

# This routine checks that the keyboard command queue is empty
# No timeout is used - if this hangs there is something wrong with
# the machine, and we probably couldn't proceed anyway.
empty_8042:
    .word 0x00eb, 0x00eb    # make a short delay
    in    $0x64, %al        # check 8042 status port
    testb $0x02, %al        # is input buffer full?
    jnz   empty_8042        # yes - loop
    ret
    
gdt:
    .word   0,0,0,0         # dummy

    .word   0x07FF          # 8Mb - limit=2047 (2048*4096=8Mb)
    .word   0x0000          # base address=0
    .word   0x9A00          # code read/exec, no privilidge, haven't been accessed
    .word   0x00C0          # granularity=4096, 386

    .word   0x07FF          # 8Mb - limit=2047 (2048*4096=8Mb)
    .word   0x0000          # base address=0
    .word   0x9200          # data read/write, haven't been accessed
    .word   0x00C0          # granularity=4096, 386

idt_48:
    .word 0                 # idt limit=0
    .word 0, 0              # idt base=0L  

gdt_48:
    .word 0x800             # gdt limit=2048, 256 GDT entries
    .word 0x200+gdt, 0x9    # gdt base = 0x90000+0x200+gdt
    
.org 2048
