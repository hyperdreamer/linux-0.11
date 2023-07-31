/*
 * The stack layout before iret is:
 * 0x17: LDT[2]: data/stack segment %esp+16
 * old %esp                         %esp+12
 * old eflags                       %esp+8
 * 0x0f: LDT[1]: code segment       %esp+4
 * offset of (1:)                   %esp
 */
#define move_to_user_mode() \
    __asm__ ("movl %%esp, %%eax\n\t" \
             "pushl $0x17\n\t" \
             "pushl %%eax\n\t" \
             "pushfl\n\t" \
             "pushl $0x0f\n\t" \
             "pushl $1f\n\t" \
             "iret\n" \
             "1:\n\t" \
             "movl $0x17, %%eax\n\t" \
             "movw %%ax, %%ds\n\t" \
             "movw %%ax, %%es\n\t" \
             "movw %%ax, %%fs\n\t" \
             "movw %%ax, %%gs\n\t" \
             : \
             : \
             :"%eax" \
            )

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::)

/*
 * set idt descriptor
 * 
 * %0: (the 3rd word of a gate) p=1, dpl=0, type(5bits)
 * %1: the address (offset) of low 32 bits of the gate
 * %2: the address (offset) of high 32 bits of the gate
 * %3: the address of interrupt process program
 * %4: high 16 bits of %eax == selector(0x8, GDT[1]) with dpl=00,ti=0(gdt), index=1
 */ 
#define _set_gate(gate_addr, type, dpl, addr) \
    __asm__ ("movw %%dx, %%ax\n\t" \
             "movw %0, %%dx\n\t" \
             "movl %%eax, %1\n\t" \
             "movl %%edx, %2\n\t" \
             : \
             : \
             "i" (0x8000+(dpl<<13)+(type<<8)), \
             "m" (gate_addr[0]), \
             "m" (gate_addr[4]), \
             "d" (addr), \
             "a" (0x00080000) \
            )

// type=14(01110), dpl=0
#define set_intr_gate(n, addr) \
	_set_gate( ((char*)(idt + n)), 14, 0, (unsigned long) addr)

// type=15(01111), dpl=0
#define set_trap_gate(n, addr) \
	_set_gate( ((char*)(idt + n)), 15, 0, (unsigned long) addr)

// type=15(01111), dpl=3
#define set_system_gate(n, addr) \
	_set_gate( ((char*)(idt + n)), 15, 3, (unsigned long) addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) |    \
		((limit) & 0x0ffff); }
/*
 * set tss or ldt descriptor (in gdt)
 * 
 * %0 : address
 * %1 : limit low 2bytes
 * %2 : base low 2bytes
 * %3 : base middle 1byte
 * %4 : type, dpl, P 1byte
 * %5 : limit high 4bits
 * %6 : base high 1byte
 *
 * pay atten to the $"type", double quote is important
 */
/* Some bugs of limit setting. 104 bytes are not enough for tss_struct which has
 * 212 bytes, but too big for ldt which has 24 bytes. So a patch is needed. */
#define _set_tssldt_desc(n, addr, type) \
    __asm__ ("movl %0, %%eax\n\t" \
             "movw $104, %1\n\t" \
             "movw %%ax, %2\n\t" \
             "rorl $16, %%eax\n\t" \
             "movb %%al, %3\n\t" \
             "movb $" type ", %4\n\t" \
             "movb $0x00, %5\n\t" \
             "movb %%ah, %6\n\t" \
             "rorl $16, %%eax\n\t" \
             : \
             : \
             "r"(addr), \
             "m"(n[0]), \ 
             "m"(n[2]), \
             "m"(n[4]), \
             "m"(n[5]), \
             "m"(n[6]), \
             "m"(n[7]) \
             : \
             "%eax" \
            )
/*
 * 0x89 = 1,00,01001; p = 1; dpl = 0; type = 01001 (busy = 0)
 * 0x82 = 1,00,00010; p = 1; dpl = 0; type = 00010
 */ 
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)

#define set_tss_desc(nr, addr) \
    _set_tssldt_desc( ((char*) (gdt + (nr<<1) + FIRST_TSS_ENTRY)), \
                      (unsigned long) addr, "0x89")

#define set_ldt_desc(nr, addr) \
    _set_tssldt_desc( ((char*) (gdt + (nr<<1) + FIRST_LDT_ENTRY)), \
                      (unsigned long) addr, "0x82")
