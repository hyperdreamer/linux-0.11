/*
 * outb, intb:      version without delay
 * outb_p, intb_:   version with delay
 */
#define outb(value,port) \
    __asm__ ("outb %%al, %%dx\n\t" \
             : \
             : \
             "a" (value), \
             "d" (port) \
            )

#define inb(port) \
    ({ \
     unsigned char _v; \
    __asm__ ("inb %%dx, %%al\n\t" \
             : \
             "=&a"(_v) \
             : \
             "d"(port) \
            ); \
     _v; \
    })

#define outb_p(value,port) \
    __asm__ __volatile__("outb %%al, %%dx\n\t" \
                         "jmp 1f\n" \
                         "1:\n\t" \
                         "jmp 1f\n" \
                         "1:\n\t" \
                         : \
                         : \
                         "a"(value), \
                         "d"(port) \
                        )

#define inb_p(port) \
    ({ \
     unsigned char _v; \
    __asm__ __volatile__("inb %%dx, %%al\n\t" \
                         "jmp 1f\n" \
                         "1:\n\t" \
                         "jmp 1f\n" \
                         "1:\n\t" \
                         : \
                         "=&a"(_v) \
                         : \
                         "d"(port) \
                        ); \
     _v; \
    })

#ifdef DEBUG
inline void poll_parallel_busy() {
    __asm__ ("1:\n\t"
             "int $0x379, %%al\n\t"
             "andb $0x80, %%al\n\t"
             "compb $0, %%al\n\t"
             "jnz 1b\n\t"
             :
             :
             :"%eax"
            );
}

inline void string_to_parallel(const char* string, int len) 
{
    for (int i = 0; i < len; i++) {
        outb(string[i], 0xe9);  // bochs' 0xe9 hack
        poll_parallel_busy();
        outb(string[i], 0x378);
        // telling the controller the data is ready
        __asm__ ("inb $0x37a, %%al\n\t"
                 "orb $1, %%al\n\t"
                 "outb %%al, $0x37a\n\t"
                );
    }
}
#endif
