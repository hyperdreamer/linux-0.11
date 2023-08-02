/*
 *  linux/kernel/asm.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * asm.s contains the low-level code for most hardware faults.
 * page_exception is handled by the mm,  so that isn't here. This
 * file also handles (hopefully) fpu-exceptions due to TS-bit,  as
 * the fpu must be properly saved/resored. This hasn't been tested.
 */

.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, coprocessor_error, irq13, reserved
# Warning: If you don't wanna mess out the stack layout, explictly use 
# "pushl -- popl" instead of "push -- pop "
                                # stack layout:
                                # esp+60: old ss
                                # esp+56: old esp
                                # esp+52: old eflags
                                # esp+48: old cs 
                                # esp+44: old eip
divide_error:                   # from here above are pushed by the processor
	pushl $do_divide_error      # esp+40
no_error_code:
	xchgl %eax, (%esp)          # %eax <-> (%esp),  %eax = &function
	pushl %ebx                  # esp+36
	pushl %ecx                  # esp+32
	pushl %edx                  # esp+28
	pushl %edi                  # esp+24
	pushl %esi                  # esp+20
	pushl %ebp                  # esp+16
	pushl %ds                   # esp+12
	pushl %es                   # esp+8
	pushl %fs                   # esp+4 :-),  the increment is 4
	pushl $0                    # "error code" as 0, the second parameter for call
	leal 44(%esp), %edx         # store old eip into %edx
	pushl %edx                  # old eip address, the first parameter for call
    ## movl $0x10, %edx         # use data segment
	movl $2<<3, %edx            # use data segment GDT[2] :-)
	movw %dx, %ds               # update %ds,  %es,  %fs
    movw %dx, %es
	movw %dx, %fs
	call *%eax   # call the real precess entity whose address was stored in %eax
	addl $8, %esp
	popl %fs
	popl %es
	popl %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

debug:
	pushl $do_int3		# do_debug
	jmp no_error_code

nmi:
	pushl $do_nmi
	jmp no_error_code

int3:
	pushl $do_int3
	jmp no_error_code

overflow:
	pushl $do_overflow
	jmp no_error_code

bounds:
	pushl $do_bounds
	jmp no_error_code

invalid_op:
	pushl $do_invalid_op
	jmp no_error_code

coprocessor_segment_overrun:
	pushl $do_coprocessor_segment_overrun
	jmp no_error_code

reserved:
	pushl $do_reserved
	jmp no_error_code

irq13:
	pushl %eax
	xorb %al, %al
	outb %al, $0xF0
	movb $0x20, %al
	outb %al, $0x20
	jmp 1f
1:	
    jmp 1f
1:	
    outb %al, $0xA0
	popl %eax
	jmp coprocessor_error

                                # stack layout:
                                # esp+60: old ss
                                # esp+56: old esp
                                # esp+52: old eflags
                                # esp+48: old cs 
                                # esp+44: old eip
                                # esp+40: error code
double_fault:                   # from here above are pushed by the processor
	pushl $do_double_fault      # esp+36
error_code:
	xchgl %eax, 4(%esp)         # esp+36: error code <-> %eax
	xchgl %ebx, (%esp)          # esp+36: &function <-> %ebx
	pushl %ecx                  # esp+32
	pushl %edx                  # esp+28
	pushl %edi                  # esp+24
	pushl %esi                  # esp+20
	pushl %ebp                  # esp+16
	pushl %ds                   # esp+12
	pushl %es                   # esp+8
	pushl %fs                   # esp+4
	pushl %eax                  # error code (esp+0), the second parameter for call
	leal 44(%esp), %eax         # get return address
	pushl %eax                  # return address: the first parameter for call
	## movl $0x10, %eax
    movl $2<<3, %eax            # use data segment,  GDT[2]
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	call *%ebx
	addl $8, %esp
	popl %fs
	popl %es
	popl %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

invalid_TSS:
	pushl $do_invalid_TSS
	jmp error_code

segment_not_present:
	pushl $do_segment_not_present
	jmp error_code

stack_segment:
	pushl $do_stack_segment
	jmp error_code

general_protection:
	pushl $do_general_protection
	jmp error_code
