/*
 *  linux/kernel/system_call.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  system_call.s  contains the system-call low-level handling routines.
 * This also contains the timer-interrupt handler, as some of the code is
 * the same. The hd- and flopppy-interrupts are also here.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call. Ordinary interrupts
 * don't handle signal-recognition, as that would clutter them up totally
 * unnecessarily.
 *
 */

SIG_CHLD	= 17

EAX		= 0x00
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28
OLDSS		= 0x2C

state	= 0		# these are offsets into the task-struct.
counter	= 4
priority = 8
signal	= 12
sigaction = 16		# MUST be 16 (=len of sigaction)
blocked = (33*16)

# offsets within sigaction
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12

nr_system_calls = 72

/*
 * Ok, I get parallel printer interrupts while using the floppy for some
 * strange reason. Urgel. Now I just ignore them.
 */
 # Warning: To keep the stack layout well-aligned, use "pusl -- popl"
 # instead of "push -- pop" by Henry
.globl system_call, sys_fork, timer_interrupt, sys_execve
.globl hd_interrupt, floppy_interrupt, parallel_interrupt
.globl device_not_available, coprocessor_error

#.align 2
.p2align 2
bad_sys_call:
	movl $-1, %eax
	iret

#.align 2
.p2align 2
reschedule:
	pushl $ret_from_sys_call
	jmp schedule

#.align 2
.p2align 2
system_call:
	cmpl $nr_system_calls-1, %eax
	ja bad_sys_call
	pushl %ds
	pushl %es
	pushl %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	movl $0x10, %edx    # %ds = %es = $0x10: gdt[1]
	movw %dx, %ds
	movw %dx, %es
	movl $0x17, %edx    # %fs = %0x17: ldt[2]		
	movw %dx, %fs        
	call sys_call_table(,%eax,4)
	pushl %eax          # return value of system call
	movl current, %eax
	cmpl $0, state(%eax)	# is the current process runnable?
	jne reschedule          # if not, then reschedule
	cmpl $0, counter(%eax)	# counter
	je reschedule
/*
 * Stack layout before 'ret_from_system_call':
 *
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip             # the rest are used by iret
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */
ret_from_sys_call:
	movl current, %eax		    # task[0] have no signals
	cmpl task, %eax
	je 3f                       # if current == task[0], then jump
                                # 0x0f: user-mode code segment: ldt[1]
	cmpw $0x0f, CS(%esp)		# was old code segment supervisor ?
	jne 3f                      # if it was supervisor, then jump
                                # 0x17: user-mode data segment: ldt[2]
	cmpw $0x17, OLDSS(%esp)		# was old stack segment sumpervisor ?
	jne 3f                      # if it was supervisor, then jump

	movl signal(%eax), %ebx
	movl blocked(%eax), %ecx
	notl %ecx
	andl %ebx, %ecx
	bsfl %ecx, %ecx             # bsf: to find the least significant set bit
	je 3f
	btrl %ecx, %ebx
	movl %ebx, signal(%eax)
	incl %ecx
	pushl %ecx
	call do_signal
	popl %eax
3:	
    popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	popl %fs
	popl %es
	popl %ds
	iret

/*   stack layout for jumping ret_from_sys_call
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */
#.align 2
.p2align 2
coprocessor_error:
	pushl %ds
	pushl %es
	pushl %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10, %eax            # data segment: gdt[2], dlp=0
	mov %ax, %ds                # %ds=gdt[2]
	mov %ax, %es                # %es=gdt[2]
	movl $0x17, %eax            # data segment: ldt[2], dlp=3 (user-mode)
	mov %ax, %fs                # %fs=ldt[2]
	pushl $ret_from_sys_call
	jmp math_error

#.align 2
.p2align 2
device_not_available:
	pushl %ds
	pushl %es
	pushl %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	clts				# clear TS so that we can use math
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)
	je math_state_restore
	pushl %ebp
	pushl %esi
	pushl %edi
	call math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret

#.align 2
.p2align 2
timer_interrupt:
	pushl %ds		# save ds,es and put kernel data space
	pushl %es		# into them. %fs is used by _system_call
	pushl %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	incl jiffies
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)
	pushl %eax
	call do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call

#.align 2
.p2align 2
sys_execve:
	lea EIP(%esp),%eax
	pushl %eax
	call do_execve
	addl $4,%esp
	ret

#.align 2
.p2align 2
sys_fork:
	call find_empty_process
	testl %eax, %eax    # %eax == nr: Empty task slot
	js 1f
	pushl %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process
	addl $20, %esp
1:	ret

hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ds
	pushl %es
	pushl %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	xorl %edx,%edx
	xchgl do_hd,%edx
	testl %edx,%edx
	jne 1f
	movl $unexpected_hd_interrupt,%edx
1:	outb %al,$0x20
	call *%edx		# "interesting" way of handling intr.
	popl %fs
	popl %es
	popl %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

floppy_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ds
	pushl %es
	pushl %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0x20		# EOI to interrupt controller #1
	xorl %eax,%eax
	xchgl do_floppy,%eax
	testl %eax,%eax
	jne 1f
	movl $unexpected_floppy_interrupt,%eax
1:	call *%eax		# "interesting" way of handling intr.
	popl %fs
	popl %es
	popl %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

parallel_interrupt:
	pushl %eax
	movb $0x20, %al
	outb %al, $0x20
	popl %eax
	iret
