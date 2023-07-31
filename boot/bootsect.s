# SYS_SIZE is the number of clicks (15 bytes) to be loaded.
# 0x3000 is 0x30000 bytes = 192kB, more than enough for current
# versions of linux
#
#
#       bootsect.s              (C) 1991 Linus Torvalds
#                               (C) 2009 He Wen
#
# bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
# iself out of the way to address 0x90000, and jumps there.
#
# It then loads 'setup' directly after itself (0x90200), and the system
# at 0x10000, using BIOS interrupts. 
#
# NOTE! currently system is at most 8*65536 bytes long. This should be no
# problem, even in the future. I want to keep it simple. This 512 kB
# kernel size should be enough, especially as this doesn't contain the
# buffer cache as in minix
#
# The loader has been made as simple as possible, and continuos
# read errors will result in a unbreakable loop. Reboot by hand. It
# loads pretty fast by getting whole sectors at a time whenever possible.

.code16
.section .text

.equ SYSSIZE,   0x3000                  # 3 * 64KB = 192KB = 384 sectors
.equ SETUPLEN,  4                       # nr of setup-sectors
.equ BOOTSEG,   0x07c0                  # original address of boot-sector
.equ INITSEG,   0x9000                  # we move boot here - out of the way
.equ SETUPSEG,  0x9020                  # setup starts here
.equ SYSSEG,    0x1000                  # system loaded at 0x10000 (65536).
.equ ENDSEG,    SYSSEG + SYSSIZE        # where to stop loading

# ROOT_DEV:     0x000 - same type of floppy as boot.
#               0x301 - first partition on first drive etc
#.equ ROOT_DEV, 0x306
.equ ROOT_DEV, 0x301

    ljmp $BOOTSEG, $start

start:
    movw $BOOTSEG, %ax                  # The MOV instruction cannot be used 
    movw %ax, %ds                       # to move a value directly to a segment register.
    movw $INITSEG, %ax
    movw %ax, %es

    xorw %si, %si
    xorw %di, %di
    movw $256, %cx                      # 512 bytes, 1 sector of a floppy disk
    cld
    rep movsw
    ljmp $INITSEG, $go
go: 
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es
# put stack at 0x9ff00.
    movw %ax, %ss
    movw $0xff00, %sp                    # aribtary value >> 512

# load the setup-sectors directly after the bootblock. (4 sectors = 2KB)
# Note that 'es' is already set up.
# reminder: %ds=%es=%cs=$INITSEG=0x9000

load_setup:
    movw $0x0200+SETUPLEN, %ax          # ah:02 to read; al:nr of sectors to read (SETUPLEN=4)
    movw $0x0200, %bx                   # write to es:bx (0x0200=512byte, directly after bootsect)
    movw $0x0002, %cx                   # ch:cylinder (differs from track) , cl:start sector (here 2)
    movw $0x0000, %dx                   # dh:head, dl:drivers
    int  $0x13
    jnc  ok_load_setup                  # CF=0 means the process acomplished
    xorw %ax, %ax                       # reset disk
    xorw %dx, %dx                       # is it necessary
    int  $0x13
    jmp  load_setup

# Get disk drive parameters, specifically nr of sectors/track

ok_load_setup:
    movb $0x00, %dl             # DL=0 is the first floppy driver
    movw $0x0800, %ax           # AH=8 is to get drive parameters
    int  $0x13
    ## 
    ## movb $0x00, %ch          # CH is the last cylinder index (0~1023, nr of cylinders -1)
    ## masking higher 10 bits is more correct than the old code
    andw $0x003F, %cx           # get sectors/track, maximum 2^6-1=63
    ## 
    movw %cx, %cs:sectors
    movw $INITSEG, %ax          # the es has been changed :-)
    movw %ax, %es               # you should reset it

# Print some inane message

    movb $0x03, %ah             # read cursor pos
    xorb %bh, %bh               # BH=0 is the page nr
    int  $0x10

    movw $24, %cx               # the length of the string is stored in $cx
    movw $0x0007, %bx           # page 0, attribute 7 (normal)
    movw $msg1, %bp
    movw $0x1301, %ax           # write string, move cursor
    int  $0x10

# ok, we've written the message, now
# we want to load the system (at 0x10000, $SYSSEG=0x1000)

    movw $SYSSEG, %ax
    movw %ax, %es               # segment of 0x10000
    call read_it
    call kill_motor

# After that we check which root-device to use. If the device is
# defined (!= 0), nothing is done and the given device is used.
# Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
# on the number of sectors that the BIOS reports currently.

    movw %cs:root_dev, %ax       
    cmpw $0x0000, %ax
    jne  root_defined
    movw %cs:sectors, %bx       # %cs:sectors (sectors/track)
    movw $0x0208, %ax
    cmpw $15, %bx               # /dev/at0 - 1.2Mb
    je   root_defined
    movw $0x021c, %ax
    cmpw $18, %bx               # /dev/PS0 - 1.44Mb
    je root_defined
undef_root:
    jmp undef_root              # dead loop
root_defined:
    movw %ax, %cs:root_dev

# after that (everyting loaded), we jump to
# the setup-routine loaded directly after
# the bootblock:
    ljmp $SETUPSEG, $0


# This routine loads the system at address 0x10000, making sure
# no 64kB boundaries are crossed. We try to load it as fast as
# possible, loading whole tracks whenever we can.
#
# in:   es - starting address segment (normally 0x1000)
#
sread:
    .word 1+SETUPLEN            # sectors read of current track ($SETUPLEN=4)
cylinder:
    .word 0                     # current track
head:
    .word 0                     # current head
sectors:
    .word 0                     # sectors (sectors/track)

# Before entering the "read_it:" function, keep in mind that %cs:sectors=sectors/track
    
read_it:
    movw %es, %ax
    test $0x0fff, %ax           # test: logical and, result not saved, eflag changed
                                # if the logical and value is 0, then ZF is set
die:
    jne  die                    # es must be at 64kB boundary, otherwise dma error occurs
    
    xorw %bx, %bx               # bx is the starting address for writing data
rp_read:
    movw %es, %ax               # $ENDSEG = $SYSSEG + $SYSSIZE = 0x1000 + 0x3000 
    cmpw $ENDSEG, %ax           # have we loaded all yet?
    jb   ok1_read               # not yet (%ax < $ENDSEG)
    ret
    
ok1_read:                       # test how many bytes can be filled in current segment
    movw %cs:sectors, %ax       # %cs:sectors holds the sector parameter of a floppy disk 
    subw sread, %ax             # %ax = nr of sectors left in the current track 
    movw %ax, %cx               #
    shlw $9, %cx                # bytes need to be read, %cx * (512byte/sector),  maximum 32256B < 32kB
    addw %bx, %cx			          # within 64kB boundry test, unsigned integers addition (%bx, starting)
    jnc  ok2_read               # it is within 64kB boundary
    je   ok2_read				        # it is just at 64kB boundary
    xorw %ax, %ax	              # %ax=0, but looks like 64kB
    subw %bx, %ax               # %ax is the bytes can be read (an unsigned integer, not a negative one)
    shrw $9, %ax				        # logical right shift: %ax/512 = nr of sectors allowed
ok2_read:
    call read_track             # read track
    movw %ax, %cx               # %cx=%ax= nr of sectors has been read
    addw sread, %ax             # Test if there is any sectors left in the current track.
    cmpw %cs:sectors, %ax		    # If it's yes, jump to ok3_read. (sectors: sectors/track),
    jne  ok3_read               # (read those left first)
    movw $1, %ax                # Otherwise the whole track has been read. Only 2 heads (0-1) are needed.
    subw head, %ax              # Test if a new cylinder should be read. If yes, use another
    jne  ok4_read               # head and jump to ok4_read.
    incw cylinder               # Otherwise, prepare to move onto a new cylinder
ok4_read:
    movw %ax, head              # If a new cylinder will be read, then "head" will be 0, otherwise 1.
    xorw %ax, %ax
ok3_read:                       # update segment and starting address
    movw %ax, sread             # If a whole track is finished, then "sread" will be 0.
    shlw $9, %cx                # Otherwise, "sread" will increase; %cx saves the bytes has been read
    addw %cx, %bx               # in last reading process; %bx the starting address will increase
    jnc  rp_read                # If the current segment is not full, then jump to rp_read.
    movw %es, %ax               # Otherwise, use the next segment (64kb) to write.
    addw $0x1000, %ax           #
    movw %ax, %es               #
    xorw %bx, %bx               # Remember to initiate the starting address before writing the new
    jmp  rp_read                # segment.

read_track:                     # save registers first
    pushw %ax                   # %ax nr of sectors allowed
    pushw %bx
    pushw %cx
    pushw %dx
1:  
    movw cylinder, %dx          # "cylinder" saves the current cylinder No. (10-bit)
    movw sread, %cx             # "sread" saves the nr. (1-63) of sectors having been read 
    incw %cx                    # %cx (the low 0-5 bit) gets starting sector to read
    movb %dl, %ch               # %ch saves the low 8 bits of "cylinder"
    ##
    shlb $6, %dh                # get the high 2 bits of the cylinder No.
    orb %dh, %cl				        # set the (the low 6-7 bit) of %cl
    ## 
    ## movw head, %dx              # "head" saves the current head no.
    ## movb %dl, %dh               # DH is the head nr
    ## 
    movb head, %dh              # my change. head (0-255)
    ## 
    ## movb $0x00, %dl             # DL is the driver
    ## 
    andw $0x0100, %dx           # (0,1) heads
    movb $0x02, %ah             # %al nr of sectors to read has been set in "ok1_read:"
    int  $0x13                  # CF = 0: Operation successful; CF = 1: Operation failed
    jc   bad_rt                 # reset the disk, then read_track again
    
    popw %dx
    popw %cx
    popw %bx
    popw %ax
    ret
bad_rt:
    xorb %ah, %ah               # reset disk
    xorb %dl, %dl               # driver nr
    int  $0x13

    ## popw %dx                    
    ## popw %cx
    ## popw %bx
    ## popw %ax
    ## jmp  read_track

    jmp 1b                      # my modification

/*
 * This procedure turns off the floppy drive motor, so
 * that we enter the kernel in a known state, and
 * do not have to worry about it later.
 */
kill_motor:
    pushw %ax
    pushw %dx
    movw $0x03f2, %dx
    movb $0x00, %al
    out  %al, %dx
    popw %dx
    popw %ax
    ret

debug_msg: # length = 16
    .byte 13,10                 # '\r'=13, '\n'=10
    .ascii "DEBUG :-)..."
    .byte 13,10

msg1:
    .byte 13,10
    .ascii "Loading system ..."
    .byte 13,10,13,10

.org 508
root_dev:
    .word ROOT_DEV
boot_flag:                      # the magic number stored at the end of the MBR
    .word 0xAA55                # The BIOS use the magic number to identify the
                                # end of the MBR and start of the OS
