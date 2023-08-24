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

int new_block(int dev)
{
    struct super_block* sb = get_super(dev);
    if (!sb) panic("trying to get new block from nonexistant device");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    struct buffer_head* bh;
    int i, j;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 

    for (i = 0; i < sb->s_zmap_blocks; ++i) {
        bh = sb->s_zmap[i];
#ifdef DEBUG
        if (!bh) {
            printkc("new_block: Dev %#x: Filesystem is corrupted!\n", dev);
            panic("new_block: Filesystem is corrupted!");
        }
#endif
        j = find_first_zero(bh->b_data);        // TO_READ
        if (j < BLCK_BITS) break;
    }   // find the nr of the first emtpy zone
    /***************************************************************/
#ifdef DEBUG
    if (j >= BLCK_BITS && i < sb->s_zmap_blocks) {
        printkc("new_block: Something wrong with the bitmap searching!\n");
        panic("new_block: Something wrong with the bitmap searching!");
    }
#endif
    // the 0 zone is the boot block. If you get it, the disk is full.
    if (i == sb->s_zmap_blocks) {
#ifdef DEBUG
        printkc("new_block: Dev %#x: run out of data zones!\n", dev);
#endif
        return 0;
    }
    //////////////////////////////////////////////////////////////////////////
    if (set_bit(j, bh->b_data)) panic("new_block: bit already set!");
    /***************************************************************/
    bh->b_dirt = 1;
    // why -1 ? Check the debugging info in read_super()
    j += i * BLCK_BITS + sb->s_firstdatazone - 1;
    /***************************************************************/
    if (j >= sb->s_nzones) return 0;    // crutical!!!
    /***************************************************************/
    // getblk() here acutually finds a empty buffer block
    bh = getblk(dev, j);
    if (!bh || bh->b_count != 1) {
#ifdef DEBUG
        printkc("new_block: Somthing wrong with the getblk!\n"
                "Before panic, we'd better reset the zone bitmap bit.\n"
                "Otherwise it will cause a filesystem inconsistency!\n");
#endif
        /******************************************************/
        struct buffer_head* tmp = sb->s_zmap[i];
        j -= i * BLCK_BITS + sb->s_firstdatazone - 1;
        /******************************************************/
        if (clear_bit(j, tmp->b_data))
            panic("new_block: bit already cleared!");
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
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 

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
    bh = sb->s_zmap[ZMAP_INDX(block)];
    /***************************************************************/
    if (clear_bit(block & BLCK_MASK, bh->b_data)) {
        printk("block (%04x:%d) ", dev, block + sb->s_firstdatazone - 1);
        panic("free_block: bit already cleared");
    }
    /***************************************************************/
    bh->b_dirt = 1;
}

struct m_inode* new_inode(int dev)
{
    struct m_inode* inode = get_empty_inode();
    if (!inode) return NULL;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    struct super_block* sb = get_super(dev);
    if (!sb) panic("new_inode with unknown device");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    struct buffer_head* bh;
    int i, j;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 

    for (i = 0; i < sb->s_imap_blocks; ++i) {
        bh = sb->s_imap[i];
#ifdef DEBUG
        if (!bh) {
            printkc("new_inode: Dev %#x: Filesystem is corrupted!\n", dev); 
            panic("new_inode: Filesystem is corrupted!");
        } 
#endif
        j = find_first_zero(bh->b_data);
        if (j < BLCK_BITS) break;
    }   // find the nr of the first emtpy inode
    /***************************************************************/
#ifdef DEBUG
    if (j >= BLCK_BITS && i < sb->s_imap_blocks) {
        printkc("new_inode: Something wrong with the bitmap searching!\n");
        panic("new_inode: Something wrong with the bitmap searching!");
    }
#endif
    /***************************************************************/
    if (i == sb->s_imap_blocks) {
#ifdef DEBUG
        printkc("new_inode: Dev %#x: run out of i-nodes!\n", dev);
#endif
        iput(inode);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    if (set_bit(j, bh->b_data)) panic("new_node: bit already set");
    /***************************************************************/
    bh->b_dirt = 1;
    j += i * BLCK_BITS;
    if (j > sb->s_ninodes) {
        iput(inode);
        return NULL;
    }
    /***************************************************************/
    //inode->i_count = 1;   // already set by get_empty_inode()
    inode->i_nlinks = 1;
    inode->i_dev = dev;
    inode->i_uid = current->euid;
    inode->i_gid = current->egid;
    inode->i_dirt = 1;
    inode->i_num = j;
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    /***************************************************************/
    return inode;
}

void free_inode(struct m_inode* inode)
{
	if (!inode) return;
    /***************************************************************/
    int inr = inode->i_num;
    if (inr < 1) panic("tring to free inode 0 or negative inodes");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if (!inode->i_dev) {
		memset(inode, 0, sizeof(*inode));
		return;
	}
    /***************************************************************/
	if (inode->i_count > 1) {
		printk("trying to free inode with count=%d\n",inode->i_count);
		panic("free_inode");
	}
    /***************************************************************/
	if (inode->i_nlinks) panic("trying to free inode with links");
    //////////////////////////////////////////////////////////////////////////
	struct super_block* sb = get_super(inode->i_dev);
	if (!sb) panic("trying to free inode on nonexistent device");
	if (inr > sb->s_ninodes) panic("trying to nonexistant inode");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	struct buffer_head* bh = sb->s_imap[IMAP_INDX(inr)];
	if (!bh) panic("nonexistent imap in superblock");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 

	if (clear_bit(inr & BLCK_MASK, bh->b_data))
		printk("free_inode: bit already cleared.\n\r");
    /***************************************************************/
	bh->b_dirt = 1;
    /***************************************************************/
	memset(inode, 0, sizeof(*inode));
}

