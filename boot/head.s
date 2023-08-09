/*
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 *  (C) 2023  Henry Wen
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x0000_0000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */

.text
.globl startup_32, idt, gdt, pg_dir, tmp_floppy_area

startup_32:
pg_dir:
	movl $0x10, %eax        # index: 0b10 == GDT[2] (data segment)
                            # TI == 0 (GDT), RPL == 0b00
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    lss stack_start, %esp   # stack_start is defined in kernel/sched.c 
                            # :tag stack_start to check it. the stack is 4KB

    call setup_idt
    call setup_gdt

    movl $0x10, %eax		    # reload all the segment registers
    mov %ax, %ds		        # after changing gdt. CS was already
    mov %ax, %es		        # reloaded in 'setup_gdt'
    mov %ax, %fs
    mov %ax, %gs
    lss stack_start, %esp
    xorl %eax, %eax
1:	
    incl %eax		    # check that A20 really IS enabled
    movl %eax,0x000000	# loop forever if it isn't
    cmpl %eax,0x100000
    je 1b

/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
	movl %cr0, %eax		    # check math chip
	andl $0x80000011, %eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2, %eax		    # set MP
	movl %eax, %cr0
    call check_x87
    jmp after_page_tables

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
check_x87:
    fninit
    fstsw %ax       # store the current state of the FPU
    cmpb $0, %al 
	je 1f			      /* no coprocessor: have to set bits */
	movl %cr0, %eax
    xorl $6, %eax		/* reset MP, set EM */
    movl %eax,%cr0
	ret

#.align 2
.p2align 2
1:  # if the execution flow gets here, no coprocessor exists	
    .byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */
	ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
setup_idt:
	lea ignore_int, %edx        # load the effective address into %dex
                                # the effective address is just the offset 
                                # of the segment model

    # Note that %eax will be the low 32-bit of an interrupt descriptor
    movl $0x00080000, %eax      # 0x0008_0000
    movw %dx, %ax		        /* selector == 0x0008 == GDT[1] == code segment */
                            # now the high 16-bit of %eax is the selector
                            # the low 16-bit of %eax is low 16-bit of the
                            # effecitve address of  ignore_int

    # Note that %edx will be the high 32-bit of an interrupt descriptor
    # the high 16-bit of %edx is the high 16-bit of the effecitve address
    # of ignore_int set by the instruction lea
    movw $0x8E00, %dx	      /* interrupt gate - dpl=0, present */

    lea idt, %edi
    mov $256, %ecx

rp_sidt:
    movl %eax, (%edi)
    movl %edx, 4(%edi)
    addl $8, %edi
    dec %ecx                # vector numbers: 0 ~ 255
    jne rp_sidt
    lidt idt_descr
    ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
setup_gdt:
	lgdt gdt_descr
	ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 * 
 * Each page table is 4KB in size. Since each entry is 4 bytes,
 * we have 1024 entries. Since the page granuality is 4KB, each 
 * page table can cover 4KB * 1024 = 4MB memory area.
 *
 * The high 10 bits {31:22} of an linear address is used as 
 * the page table directory. (Max entries: 1024). Since we only
 * have 16 MB, we use 4 entries. (1 PDE entries overs 4MB space.)
 * Later you will see that the 4KB before pg0 are used to store the
 * page directory. Check pg_dir at the head of the file
 *
 * The next 10 bits {21:12} is used as a page table index.
 * So 1024 entries!!!
 */
.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it is not
 * on a 64kB border.
 */
tmp_floppy_area:   # Size: 1KB
	.fill 1024,1,0

after_page_tables:
	pushl $0		    # These are the parameters to main :-)
	pushl $0
	pushl $0
	pushl $L6		    # return address for main(), if it decides to.
	pushl $main         # init/main.c
	jmp setup_paging
L6:
	jmp L6			    # main should never return here, but
				        # just in case, we know what happens.

/* This is the default interrupt "handler" :-) */
int_msg:
	.asciz "Unknown interrupt\n\r" # .asciz:
                                 # string ended with '\0' like C/C++

#.align 2
.p2align 2
ignore_int:
    pushl %eax
    pushl %ecx
    pushl %edx
    pushw %ds
    pushw %es
    pushw %fs

    movl $0x10, %eax    # GDT[2]: data segment
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    pushl $int_msg
    call printk        # :tag printk :-)

    popl %eax
    popw %fs
    popw %es
    popw %ds
    popl %edx
    popl %ecx
    popl %eax
    iret                # return from interrup

/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won\'t guarantee that's all :-( )
 */
#.align 2
.p2align 2
setup_paging:
    # intiate all to zero
    movl $1024*5, %ecx		  /* 5 pages - pg_dir+4 page tables */
    xorl %eax, %eax
    xorl %edi, %edi			    /* pg_dir is at 0x000 */
    cld
    rep stosl
    # Set Page Director Table: 4 entries
    movl $pg0+7, pg_dir		    /* set present bit, user mode, r/w */
    movl $pg1+7, pg_dir+4		  /*  --------- " " --------- */
    movl $pg2+7, pg_dir+8		  /*  --------- " " --------- */
    movl $pg3+7, pg_dir+12		  /*  --------- " " --------- */
    # Set Page Tables: 4 Tables, 4096 entries
    movl $pg3+4092, %edi        /* starting address, write backwards */
    movl $0xfff007, %eax		    /*  16Mb - 4096 + 7 (r/w user,p) */
    std
1:	
    stosl			      /* fill pages backwards - more efficient :-) */
    subl $0x1000, %eax
    jge 1b

    xorl %eax, %eax		/* pg_dir is at 0x0000 */
    movl %eax, %cr3		/* cr3 - page directory start */
    movl %cr0, %eax
    orl $0x80000000, %eax
	movl %eax, %cr0		/* set paging (PG) bit */
    ret			/* this also flushes prefetch-queue and jump to main() */

#.align 2
.p2align 2
.word 0
idt_descr:              # IDTR
	.word 256*8-1		# idt contains 256 entries
                        # limit size: -1 is essential
	.long idt          # the base address of IDT

#.align 2
.p2align 2
.word 0
gdt_descr:
    .word 256*8-1		# so does gdt (not that that's any
    .long gdt		    # magic number, but it works for me :^)

#.align 3
.p2align 3
idt:   # Size: 2KB	
    .fill 256,8,0		# idt is uninitialized

#.align 3
.p2align 3
gdt:	  # Size: 2KB
    .quad 0x0000000000000000	  /* NULL descriptor */
    .quad 0x00c09a0000000fff	  /* 16Mb */
    .quad 0x00c0920000000fff	  /* 16Mb */
    .quad 0x0000000000000000	  /* TEMPORARY - do not use */
    .fill 252,8,0			          /* space for LDT's and TSS's etc */
