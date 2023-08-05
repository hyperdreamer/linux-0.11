static inline void copy_block(const char* from, char* to, size_t size)
{
    __asm__ ("cld\n\t"
             "rep movsb\n\t"
             :
             :
             "S" (from),    // check system_call: %ds == %es
             "D" (to), 
             "c" (size)
            );
}

static inline void copy_block_fs2es(const char* from, char* to, size_t size)
{
    __asm__ __volatile__("pushw %%ds\n\t"
                         "movw %%fs, %%ax\n\t"
                         "movw %%ax, %%ds\n\t" // set %ds := %fs
                         "cld\n\t"
                         "rep movsb\n\t"
                         "popw %%ds\n\t"
                         :
                         :
                         "S" (from), 
                         "D" (to),              // check system_call: %ds == %es
                         "c" (size)
                         :
                         "%eax"
                        );
}

static inline void copy_block_ds2fs(const char* from, char* to, size_t size)
{
    __asm__ __volatile__("pushw %%es\n\t"
                         "movw %%fs, %%ax\n\t"
                         "movw %%ax, %%es\n\t" // set %es := %fs
                         "cld\n\t"
                         "rep movsb\n\t"
                         "popw %%es\n\t"
                         :
                         :
                         "S" (from), 
                         "D" (to), 
                         "c" (size)
                         :
                         "%eax"
                        );
}

static inline unsigned char get_fs_byte(const char* addr)
{
	unsigned char _v;

	__asm__ ("movb %%fs:%1, %0\n\t"
             : "=r" (_v)
             : "m" (*addr)
            );

	return _v;
}

static inline unsigned short get_fs_word(const unsigned short *addr)
{
	unsigned short _v;

	__asm__ ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline unsigned long get_fs_long(const unsigned long *addr)
{
	unsigned long _v;

	__asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr)); \
	return _v;
}

static inline void put_fs_byte(char val,char *addr)
{
    __asm__ ("movb %0, %%fs:%1\n\t"
             :
             :
             "r" (val),
             "m" (*addr)
            );
}

static inline void put_fs_word(short val,short * addr)
{
    __asm__ ("movw %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void put_fs_long(unsigned long val,unsigned long * addr)
{
    __asm__ ("movl %0, %%fs:%1\n\t"
             :
             :
             "r" (val),
             "m" (*addr)
            );
}

/*
 * Someone who knows GNU asm better than I should double check the followig.
 * It seems to work, but I don't know if I'm doing something subtly wrong.
 * --- TYT, 11/24/91
 * [ nothing wrong here, Linus ]
 */

static inline unsigned long get_fs() 
{
	unsigned short _v;
	__asm__("mov %%fs,%%ax":"=a" (_v):);
	return _v;
}

static inline unsigned long get_ds() 
{
	unsigned short _v;
	__asm__("mov %%ds,%%ax":"=a" (_v):);
	return _v;
}

static inline void set_fs(unsigned long val)
{
	__asm__("mov %0,%%fs"::"a" ((unsigned short) val));
}
