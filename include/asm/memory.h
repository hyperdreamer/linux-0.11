/*
 *  NOTE!!! memcpy(dest,src,n) assumes ds=es=normal data segment. This
 *  goes for all kernel functions (ds=es=kernel space, fs=local data,
 *  gs=null), as well as for all well-behaving user programs (ds=es=
 *  user data space). This is NOT a bug, as any user program that changes
 *  es deserves to die if it isn't careful.
 */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#define memcpy(dest, src, n) \
    ({ \
     void * _res = dest; \
     __asm__("cld\n\t" \
             "rep movsb\n\t" \
             : \
             : \
             "D" ((unsigned long) _res), \
             "S" ((unsigned long) (src)), \
             "c" ((size_t) (n)) \
            ); \
     _res; \
    })
