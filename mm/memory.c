/*
 *  linux/mm/memory.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

/*
 * TO READ List:
 * 
 * printk():  	print msg in kernel mode
 * 
 * do_exit(): 	task's exit process
 * 
 * panic(): 	serious kernel error
 * 
 */

#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

extern volatile int do_exit(long code);    // void --> int by Henry

static inline volatile void oom(void) /* ouot of memory function */
{
    printk("out of memory\n\r");
    do_exit(SIGSEGV);
}

// refresh the TLB [MOD] by Henry
#define invalidate() \
    __asm__("movl %0, %%cr3"::"r"(0))

/* these are not to be changed without changing head.s etc */
#define LOW_MEM 0x100000        // 1MB
#define PAGING_MEMORY (15*1024*1024)
#define PAGING_PAGES (PAGING_MEMORY>>12)
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)
#define USED 100

#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
                          current->start_code + current->end_code)

static long HIGH_MEMORY = 0;    /* initiated in mem_init */

#define copy_page(from, to) \
    __asm__ ("cld\n\t" \
             "rep movsl\n\t" \
             : \
             : \
             "S"(from), \
             "D"(to), \
             "c"(PAGE_SIZE>>2) \
            )

static unsigned char mem_map[PAGING_PAGES] = {0};

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
unsigned long get_free_page(void)
{
    unsigned long __res;

    __asm__ ("std\n\t"                  // backwards operation
             "repne scasb\n\t"
             "jne 1f\n\t"
             "movb $1, 1(%%edi)\n\t"	// set the "first" free page ocupied
             "sall $12, %%ecx\n\t"      // sall == shll
             "addl %2, %%ecx\n\t"       // %edx := real physical address of 
             "movl %%ecx, %%edx\n\t"    // the new found page	
             "movl $1024, %%ecx\n\t"    // initiate the page to 0
             "leal 4092(%%edx), %%edi\n\t"
             "rep stosl\n\t"
             "movl %%edx, %%eax\n"	    // %eax := the address of the page
             "1:\n\t"
             :
             "=a"(__res)
             :
             "0"(0), 
             "i"(LOW_MEM), 
             "c"(PAGING_PAGES),
             "D"(mem_map + PAGING_PAGES - 1)    // backwards operation
             :
             "%edx"
            );

    return __res;
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */
void free_page(unsigned long addr)
{
    if (addr < LOW_MEM) return; // Question: why cannnot use panic()?
    if (addr >= HIGH_MEMORY) panic("trying to free nonexistent page"); 
    addr -= LOW_MEM;
    addr >>= 12;            // get the number of the page that need to be freed 
    if (mem_map[addr]--) return;       // decrease the number of being used
    mem_map[addr]=0;                   // without the step, it could be -1
    panic("trying to free free page"); // panic again :-(
}

/*
 * This function frees a continuous block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks,
 * a whole page table.
 * "from" stores physical address.
 */
int free_page_tables(unsigned long from, unsigned long size)
{
    if (from & 0x3fffff) panic("free_page_tables called with wrong alignment");
    if (!from) panic("Trying to free up swapper memory space");
    //////////////////////////////////////////////////////////////////////////
    size = (size + 0x3fffff) >> 22; // the nr of 4Mb blocks need to be freed
    unsigned long* dir = (unsigned long*) ((from>>20) & 0xffc); 
    while (size-- > 0) {           
        if (!(1 & *dir)) {
            ++dir;
            continue;
        }
        /****************************************************/
        unsigned long* pg_table = (unsigned long*) (0xfffff000 & *dir);
        for (int nr = 0; nr < 1024; ++nr) 
        {   // test P bit 
            if (1 & *pg_table) free_page(0xfffff000 & *pg_table);
            *(pg_table++) = 0;  // [MOD] by Henry
        }
        free_page(0xfffff000 & *dir);   // free the page TABLE
        *(dir++) = 0; 
    }
    //////////////////////////////////////////////////////////////////////////
    invalidate(); // invalidate the cr3
    return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 */
int copy_page_tables(unsigned long from, unsigned long to, unsigned long size)
{
    unsigned long* from_page_table;
    unsigned long* to_page_table;
    unsigned long* from_dir;
    unsigned long* to_dir;
    /***************************************************************/
    if ((from & 0x3fffff) || (to & 0x3fffff)) // both on 4Mb boundary :-)
        panic("copy_page_tables called with wrong alignment");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    from_dir = (unsigned long*) ((from>>20) & 0xffc); /* pg_dir = 0 */
    to_dir = (unsigned long*) ((to>>20) & 0xffc);
    // nr of page directory entries needs to copy
    for(size = ((unsigned long) (size+0x3fffff)) >> 22; 
        size-- >0;
        ++from_dir, ++to_dir) 
    {
        if (1 & *to_dir) panic("copy_page_tables: already exist");
        if (!(1 & *from_dir)) continue;
        /***************************************************************/
        // source page table address
        from_page_table = (unsigned long*) (0xfffff000 & *from_dir);
        // get a new page for dest page table
        if (!(to_page_table = (unsigned long*) get_free_page()))
            return -1;	/* Out of memory, see freeing */
        /***************************************************************/
        // u/s:1, user privilege level
        // r/w:1, read and write
        // p  :1, present now
        *to_dir = ((unsigned long) to_page_table) | 7;
        // task[0]'s LDT Discriptor Limit: 640KB: 0xA0 (160) pages
        for (register int nr = (from==0) ? 0xA0 : 1024; 
             nr-- > 0; 
             ++from_page_table, ++to_page_table) 
        {
            unsigned long this_page = *from_page_table;
            if (!(1 & this_page)) continue;
            /*******************************************************/
            this_page &= ~2; // clear R/W bit for sharing, read only
            *to_page_table = this_page; // copy the page table 
            if (this_page > LOW_MEM) 
            { // specific process for none-kernel space
                *from_page_table = this_page; // clear R/W bit for source, read only
                this_page -= LOW_MEM;	// remember to set the mem_map[] :-)
                this_page >>= 12;
                mem_map[this_page]++;
            }
        }
    }

    invalidate(); // invalidate TLB :-)
    return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */
// map a linear address to a physical page
unsigned long put_page(unsigned long page, unsigned long address)
{
    /* NOTE !!! This uses the fact that _pg_dir=0 */
    if (page < LOW_MEM || page >= HIGH_MEMORY)
        printk("Trying to put page %p at %p\n",page,address);
    if (mem_map[(page-LOW_MEM)>>12] != 1)
        printk("mem_map disagrees with %p at %p\n",page,address);
    /////////////////////////////////////////////////////////////////////////
    unsigned long* page_table = (unsigned long*) ((address >> 20) & 0xffc); 
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    if ((*page_table) & 1) // P=1, then "page_table" get the page table address
        page_table = (unsigned long*) (0xfffff000 & *page_table);
    else {
        unsigned long tmp = get_free_page();
        if (!tmp) return 0;
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        *page_table = tmp | 7; // set page dir entry and  U/S, R/W, P bits
        page_table = (unsigned long*) tmp; // get page table 
    }
    /***********************************************************************/
    // mask 0x3ff keeps the index within the bound of 10bits
    page_table[(address >> 12) & 0x3ff] = page | 7; 
    /* no need for invalidate */
    return page;
}

// copy on write :-)
void un_wp_page(unsigned long* table_entry)
{
    unsigned long old_page = 0xfffff000 & *table_entry;  // old page address
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    // mem_map[x]==1 indicates no sharing.
    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) { 
        *table_entry |= 2; // set R/W to 1
        invalidate();
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    // if the page is shared, then copy it.
    unsigned long new_page = get_free_page();
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!new_page) oom(); // run out of memory :-(
    /***************************************************************/
    if (old_page >= LOW_MEM) mem_map[MAP_NR(old_page)]--; 
    *table_entry = new_page | 7; // set U/S, R/W, P bits
    /***************************************************************/
    invalidate(); // refresh TLB
    copy_page(old_page,new_page); // copy page
}	

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * If it's in code space we exit with a segment error.
 */
void do_wp_page(unsigned long error_code, unsigned long address)
{
#if 0
    /* we cannot do this yet: the estdio library writes to code space */
    /* stupid, stupid. I really want the libc.a from GNU */
    if (CODE_SPACE(address))
        do_exit(SIGSEGV);
#endif
    // get the table entry (offset + base address of table)
    un_wp_page((unsigned long*)
               (((address>>10) & 0xffc) // offset in page table
                + (0xfffff000 & *((unsigned long*) ((address>>20) & 0xffc)))));
    // + base address of page table (stored in pg_dir)
}

void write_verify(unsigned long address)
{
    unsigned long page = *((unsigned long *) ((address>>20) & 0xffc));
    if (!(page & 1)) return; // test P bit of page directory entry
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    page &= PAGE_MASK; // extract page table address
    page += ((address>>10) & 0xffc); // add offset to get the page adress
    if ((3 & *(unsigned long*) page) == 1)  /* read-only, present */
        un_wp_page((unsigned long*) page);  // then need copy page
    return;
}

void get_empty_page(unsigned long address)
{
    unsigned long tmp;

    if (!(tmp=get_free_page()) || !put_page(tmp,address)) {
        free_page(tmp);		/* 0 is ok - ignored */
        oom();
    }
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable.
 */
static int try_to_share(unsigned long address, struct task_struct* p)
{
    unsigned long from;
    unsigned long to;
    unsigned long from_page;
    unsigned long to_page;
    unsigned long phys_addr;

    from_page = to_page = ((address>>20) & 0xffc); // page dir offset
    from_page += ((p->start_code>>20) & 0xffc);    // page dir offset for code
    to_page += ((current->start_code>>20) & 0xffc);
    /* is there a page-directory at from? */
    from = *(unsigned long *) from_page; // get the page dir entry
    if (!(from & 1))                     // test P bit
        return 0;
    from &= 0xfffff000; // get page table
    from_page = from + ((address>>10) & 0xffc); // add page table offset
    phys_addr = *(unsigned long *) from_page;   // get the page table entry
    /* is the page clean and present? */
    if ((phys_addr & 0x41) != 0x01) // test D,P bit
        return 0;                   /* P=0 or D=1 */
    phys_addr &= 0xfffff000;        // get page address
    if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM) 
        return 0;
    ////////////////////////////////////////////////////////////
    to = *(unsigned long *) to_page; /* get the page dir entry */
    if (!(to & 1)) {                 /* test P  */
        if ((to = get_free_page()))  /* P=0 then get a free page */
            *(unsigned long *) to_page = to | 7; /* set page dir entry U/S 1, R/W 1, P 1 */
        else
            oom();
    }
    to &= 0xfffff000;           /*  get page table */
    to_page = to + ((address>>10) & 0xffc); /*  get page table entry */
    ///////////////////////////////////////////////////////////
    if (1 & *(unsigned long *) to_page) // er... :-(
        panic("try_to_share: to_page already exists");
    /* share them: write-protect */
    *(unsigned long *) from_page &= ~2; // clear R/W bit
    *(unsigned long *) to_page = *(unsigned long *) from_page; // share 
    invalidate(); // remember to fresh the TLB
    
    phys_addr -= LOW_MEM;
    phys_addr >>= 12;
    mem_map[phys_addr]++; // and remember to increase the mem_map[x] :-)
    return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
static int share_page(unsigned long address)
{
    struct task_struct** p;

    if (!current->executable) return 0;
    if (current->executable->i_count < 2) return 0;
    for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
        if (!*p) continue;
        if (current == *p) continue;
        if ((*p)->executable != current->executable) continue;
        if (try_to_share(address, *p)) return 1;
    }
    return 0;
}

// the page wanted is not in memory
// you need some file system knowledge to understand the function
void do_no_page(unsigned long error_code, unsigned long address)
{
    int nr[4];
    unsigned long tmp;
    unsigned long page;
    int block,i;
    /***************************************************************/
    address &= PAGE_MASK;
    tmp = address - current->start_code;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!current->executable || tmp >= current->end_data) {
        get_empty_page(address);
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    if (share_page(tmp)) return;
    if (!(page = get_free_page())) oom();
    //////////////////////////////////////////////////////////////////////////
    /* remember that 1 block is used for header */
    block = 1 + tmp/BLOCK_SIZE;
    for (i = 0; i < 4; ++block, ++i)
        nr[i] = bmap(current->executable, block);
    /***************************************************************/
    bread_page(page, current->executable->i_dev, nr);
    //////////////////////////////////////////////////////////////////////////
    i = tmp + PAGE_SIZE - current->end_data; // if i > 0, then land on .bss   
    tmp = page + PAGE_SIZE;
    while (i-- > 0) // .bss must be zero out
        *(char*) (--tmp) = 0;   
    /***************************************************************/
    if (put_page(page, address)) return;
    free_page(page);
    oom();
}

/*
 * mark buffer area (and Ramdisk, if there is any) and pages outside
 * physical limit USED and user accessible area available
 */
void mem_init(long start_mem, long end_mem)
{
    int i;

    HIGH_MEMORY = end_mem;
    for (i = 0; i < PAGING_PAGES; i++)  // it marks all pages outside
        mem_map[i] = USED;              // physical limit USED
    i = MAP_NR(start_mem);
    end_mem -= start_mem;
    end_mem >>= 12;
    while (end_mem-- > 0)
        mem_map[i++]=0;
}

// the function is simple :-)
void calc_mem(void)
{
    int i,j,k,free=0;
    long * pg_tbl;

    for(i=0 ; i<PAGING_PAGES ; i++)
        if (!mem_map[i]) free++;
    printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
    for(i=2 ; i<1024 ; i++) {
        if (1&pg_dir[i]) {
            pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
            for(j=k=0 ; j<1024 ; j++)
                if (pg_tbl[j]&1)
                    k++;
            printk("Pg-dir[%d] uses %d pages\n",i,k);
        }
    }
}
