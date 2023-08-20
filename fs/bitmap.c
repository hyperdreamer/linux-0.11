/*
 *  linux/fs/bitmap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#define clear_block(addr) \
    __asm__ __volatile__("cld\n\t" \
                         "rep stosl\n\t" \
                         : \
                         : \
                         "a" (0), \
                         "c" (BLOCK_SIZE/4), \
                         "D" ((long) (addr)) \
                        )

#define set_bit(nr, addr) \
    ({ \
        int res; \
        __asm__ __volatile__("btsl %2, %3\n\t" \
                             "setb %%al\n\t" \
                             : \
                             "=a" (res) \
                             : \
                             "0" (0), \
                             "r" (nr), \
                             "m" (*(addr)) \
                            ); \
        res; \
    })

#define clear_bit(nr, addr) \
    ({\
        int res; \
        __asm__ __volatile__("btrl %2, %3\n\t" \
                             "setnb %%al\n\t" \
                             : \
                             "=a" (res) \
                             : \
                             "0" (0), \
                             "r" (nr), \
                             "m" (*(addr)) \
                            ); \
        res; \
    })

#define find_first_zero(addr) \
    ({ \
        int __res; \
        __asm__ __volatile__("cld\n" \
                             "1:\n\t" \
                             "lodsl\n\t" \
                             "notl %%eax\n\t" \
                             "bsfl %%eax, %%edx\n\t" \
                             "je 2f\n\t" \
                             "addl %%edx, %%ecx\n\t" \
                             "jmp 3f\n" \
                             "2:\n\t" \
                             "addl $32, %%ecx\n\t" \
                             "cmpl $8192, %%ecx\n\t" \
                             "jl 1b\n" \
                             "3:\n\t" \
                             : \
                             "=c" (__res) \
                             : \
                             "c" (0), \
                             "S" (addr) \
                            ); \
        __res; \
    })

static inline void lock_buffer(struct buffer_head* bh)
{
	cli();
	while (bh->b_lock) sleep_on(&bh->b_wait);
	bh->b_lock = 1;
	sti();
}

static inline void unlock_buffer(struct buffer_head* bh)
{
	if (!bh->b_lock) printk("buffer.c: buffer not locked\n");
    //////////////////////////////////////////////////////////////////////////
	cli();
	bh->b_lock = 0;
	wake_up(&bh->b_wait);
    sti();
}

int new_block(int dev)
{
    struct super_block* sb = get_super(dev);
    if (!sb) panic("trying to get new block from nonexistant device");
    /***************************************************************/
    struct buffer_head* bh;
    int i;
    int j;
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
repeat:
    for (i = 0; i < sb->s_zmap_blocks; ++i) {
        bh = sb->s_zmap[i];
#ifdef DEBUG
        if (!bh) {
            printkc("new_block: Dev %#x: Filesystem is corrupted!\n", 
                    sb->s_dev);
            panic("new_block: Filesystem is corrupted!");
        }
#endif
        j = find_first_zero(bh->b_data);        // TO_READ
        if (j < BLCK_BITS) break;
        /*******************************************************/
    }   // find the nr of the first emtpy zone
    /***************************************************************/
#ifdef DEBUG
    if (j >= BLCK_BITS) {
        printkc("new_block: Something wrong with the bitmap searching!\n");
        panic("new_block: Something wrong with the bitmap searching!");
    }
#endif
    // the 0 zone is the boot block. If you get it, the disk is full.
    if (i == sb->s_zmap_blocks) return 0;
    //////////////////////////////////////////////////////////////////////////
    lock_buffer(bh);
    if (set_bit(j, bh->b_data)) {
#ifdef DEBUG
        printkc("new_block: the empty zone was taken, while sleeping\n");
#endif
        unlock_buffer(bh);
        goto repeat;
    }
    /***************************************************************/
    bh->b_dirt = 1;
    // why -1 ? Check the debugging info in read_super()
    j += i * BLCK_BITS + sb->s_firstdatazone - 1;
    unlock_buffer(bh);
    /***************************************************************/
    if (j >= sb->s_nzones) return 0;    // crutical!!!
    /***************************************************************/
    // getblk() here acutually finds a empty buffer block
    bh = getblk(dev, j);
    if (!bh || bh->b_count != 1) {
        printkc("new_block: Somthing wrong with the getblk!\n"
                "Before panic, we'd better reset the zone bitmap bit.\n"
                "Otherwise it will cause a filesystem inconsistency!\n")
        /******************************************************/
        struct buffer_head* tmp = sb->s_zmap[i];
        j -= i * BLCK_BITS + sb->s_firstdatazone - 1;
        /******************************************************/
        if (clear_bit(j, tmp->b_data)) 
            printkc("new_block: Something weird happened!\n");
        /******************************************************/
        if (!bh) panic("new_block: cannot get block");
        if (bh->b_count != 1) panic("new block: count is != 1");
    }
    /***************************************************************/
    clear_block(bh->b_data);
    bh->b_uptodate = 1;
    bh->b_dirt = 1;
    brelse(bh);
    return j;
}

void free_block(int dev, int block)
{
    struct super_block* sb = get_super(dev);
    struct buffer_head* bh;
    /***************************************************************/
    if (!sb) panic("trying to free block on nonexistent device");
    if (block < sb->s_firstdatazone || block >= sb->s_nzones)
        panic("trying to free block not in datazone");
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    bh = get_hash_table(dev, block);
    if (bh) {
        if (bh->b_count != 1) {
            printk("trying to free block (%04x:%d), count=%d\n",
                   dev, block, bh->b_count);
            return;
        }
        bh->b_dirt = 0;
        bh->b_uptodate = 0;
        brelse(bh);
    }
    /***************************************************************/
    block -= sb->s_firstdatazone - 1 ;
    if (clear_bit(block & BLCK_MASK, sb->s_zmap[block/BLCK_BITS]->b_data)) {
        printk("block (%04x:%d) ", dev, block + sb->s_firstdatazone - 1);
        panic("free_block: bit already cleared");
    }
    sb->s_zmap[block/BLCK_BITS]->b_dirt = 1;
}

void free_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!inode)
		return;
	if (!inode->i_dev) {
		memset(inode,0,sizeof(*inode));
		return;
	}
	if (inode->i_count>1) {
		printk("trying to free inode with count=%d\n",inode->i_count);
		panic("free_inode");
	}
	if (inode->i_nlinks)
		panic("trying to free inode with links");
	if (!(sb = get_super(inode->i_dev)))
		panic("trying to free inode on nonexistent device");
	if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
		panic("trying to free inode 0 or nonexistant inode");
	if (!(bh=sb->s_imap[inode->i_num>>13]))
		panic("nonexistent imap in superblock");
	if (clear_bit(inode->i_num&8191,bh->b_data))
		printk("free_inode: bit already cleared.\n\r");
	bh->b_dirt = 1;
	memset(inode,0,sizeof(*inode));
}

struct m_inode * new_inode(int dev)
{
    struct m_inode * inode;
    struct super_block * sb;
    struct buffer_head * bh;
    int i,j;

    if (!(inode=get_empty_inode()))
        return NULL;
    if (!(sb = get_super(dev)))
        panic("new_inode with unknown device");
    j = 8192;
    for (i=0 ; i<8 ; i++)
        if ((bh=sb->s_imap[i]))
            if ((j=find_first_zero(bh->b_data))<8192)
                break;
    if (!bh || j >= 8192 || j+i*8192 > sb->s_ninodes) {
        iput(inode);
        return NULL;
    }
    lock_buffer(bh);
    if (set_bit(j,bh->b_data))
        panic("new_inode: bit already set");
    unlock_buffer(bh);
    bh->b_dirt = 1;
    inode->i_count=1;
    inode->i_nlinks=1;
    inode->i_dev=dev;
    inode->i_uid=current->euid;
    inode->i_gid=current->egid;
    inode->i_dirt=1;
    inode->i_num = j + i*8192;
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    return inode;
}
