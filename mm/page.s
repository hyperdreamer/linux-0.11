/*
 *  linux/mm/page.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * page.s contains the low-level page-exception code.
 * the real work is done in mm.c
 */

## .globl _page_fault
.globl page_fault

## _page_fault:
page_fault:
	xchgl %eax, (%esp)          # exchange err, store error code to %eax
	pushl %ecx
	pushl %edx
	pushw %ds
	pushw %es
	pushw %fs
	## movl $0x10, %edx			# get kernel data segment
    movl $2<<3, %edx			# get kernel data segment GDT[2]
	movw %dx, %ds               # update %ds, %es, %fs
	movw %dx, %es
	movw %dx, %fs
	movl %cr2, %edx     # %cr2 holds page fault linear address
	pushl %edx          # parameters for do_no(wp)_page		
	pushl %eax          # %edx: address, %eax: error code
	testl $1, %eax      # if error code==1 then the page wanted is not in memory
	jne 1f
    call do_no_page
	jmp 2f
1:	
    call do_wp_page
2:	
    addl $8, %esp
	popw %fs
	popw %es
	popw %ds
	popl %edx
	popl %ecx
	popl %eax
	iret					# since %ds has been changed
