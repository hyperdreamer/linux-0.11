#pragma once

#define PAGE_SIZE 4096
#define PAGE_OFFSET_MASK (PAGE_SIZE-1)
#define PAGE_MASK (~(PAGE_SIZE-1))

extern unsigned long get_free_page(void);
extern unsigned long put_page(unsigned long page, unsigned long address);
extern void free_page(unsigned long addr);

