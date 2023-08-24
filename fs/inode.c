/*
 *  linux/fs/inode.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <string.h> 
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

extern struct super_block super_block[NR_SUPER];
struct m_inode inode_table[NR_INODE] = {};
static struct task_struct* empty_inode_wait = NULL;

static inline void wait_on_inode(struct m_inode* inode)
{
    cli();
    while (inode->i_lock) {
#undef DEBUG
#ifdef DEBUG
        printkc("inode.c: wait_on_inode: Oops!\n");
#endif
        sleep_on(&inode->i_wait);
    }
    sti();
}

static inline void lock_inode(struct m_inode* inode)
{
    cli();
    while (inode->i_lock) {
#ifdef DEBUG
        printkc("inode.c: lock_inode: Oops!\n");
#endif
        sleep_on(&inode->i_wait);
    }
    inode->i_lock=1;
    sti();
}

static inline void unlock_inode(struct m_inode* inode)
{
    cli();
    inode->i_lock=0;
    wake_up(&inode->i_wait);
    sti();
}

// Only reding the inode's disk-exclusive info. 
static void read_inode(struct m_inode* inode)
{
    lock_inode(inode);
    //////////////////////////////////////////////////////////////////////////
    struct super_block* sb = get_super(inode->i_dev);
    if (!sb) panic("trying to read inode without dev");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    int block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks
                + (inode->i_num - 1)/INODES_PER_BLOCK;
    struct buffer_head* bh = bread(inode->i_dev, block);
    if (!bh) panic("read_inode(): unable to read i-node block");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    *(struct d_inode *)inode =
        ((struct d_inode *)bh->b_data)[(inode->i_num-1)%INODES_PER_BLOCK];
    /***************************************************************/
    brelse(bh); // The reading procedure doesn't occupy the inode.
    //////////////////////////////////////////////////////////////////////////
    unlock_inode(inode);
}

static void write_inode(struct m_inode* inode)
{
    lock_inode(inode);
    //////////////////////////////////////////////////////////////////////////
    if (!inode->i_dirt || !inode->i_dev) {
        unlock_inode(inode);
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    struct super_block* sb = get_super(inode->i_dev);
    if (!sb) panic("trying to write inode without device");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    int block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks
                + (inode->i_num-1)/INODES_PER_BLOCK;
    struct buffer_head* bh = bread(inode->i_dev, block);
    if (!bh) panic("write_inode(): unable to find i-node!");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    ((struct d_inode *)bh->b_data)[(inode->i_num-1)%INODES_PER_BLOCK] =
        *(struct d_inode *)inode;
    /***************************************************************/
    bh->b_dirt = 1;
    inode->i_dirt = 0;
    brelse(bh); // have to release it after write 
    //////////////////////////////////////////////////////////////////////////
    unlock_inode(inode);
}

// arg: block: the block in the i-node not in the disk
static int _bmap(struct m_inode* inode, int block, int create)
{
    if (block < 0) panic("_bmap: block < 0");
    if (block >= 7+512+512*512) panic("_bmap: block>big");
    //////////////////////////////////////////////////////////////////////////
    if (block < 7) {
        if (create && !inode->i_zone[block]) {
            if ((inode->i_zone[block] = new_block(inode->i_dev))) {
                inode->i_ctime = CURRENT_TIME;
                inode->i_dirt = 1;
            }
        }
        /***************************************************************/
        return inode->i_zone[block];
    }
    //////////////////////////////////////////////////////////////////////////
    struct buffer_head* bh;
    int i;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    block -= 7;
    if (block < 512) {
        if (create && !inode->i_zone[7]) {   // Indirect
            if ((inode->i_zone[7] = new_block(inode->i_dev))) {
                inode->i_ctime = CURRENT_TIME;
                inode->i_dirt = 1;
            }
        }
        /***************************************************************/
        if (!inode->i_zone[7]) return 0;    // new_block failed
        /***************************************************************/
        bh = bread(inode->i_dev, inode->i_zone[7]);
        if (!bh) return 0;  // if I/O error occurs
        /***************************************************************/
        i = ((unsigned short*) (bh->b_data))[block];
        if (create && !i) {
            if ((i = new_block(inode->i_dev))) {
                ((unsigned short*) (bh->b_data))[block] = i;
                bh->b_dirt = 1;
            }
        }
        brelse(bh);
        /***************************************************************/
        return i;
    }
    //////////////////////////////////////////////////////////////////////////
    block -= 512;
    if (create && !inode->i_zone[8]) {    // Double indirect
        if ((inode->i_zone[8] = new_block(inode->i_dev))) {
            inode->i_dirt=1;
            inode->i_ctime=CURRENT_TIME;
        }
    }
    /***************************************************************/
    if (!inode->i_zone[8]) return 0;    // new_block failed
    //////////////////////////////////////////////////////////////////////////
    bh = bread(inode->i_dev, inode->i_zone[8]);
    if (!bh) return 0;  // if I/O error occurs
    i = ((unsigned short*) (bh->b_data))[block >> 9];
    /***************************************************************/
    if (create && !i) {
        if ((i = new_block(inode->i_dev))) {
            ((unsigned short*) (bh->b_data))[block >> 9] = i;
            bh->b_dirt = 1;
        }
    }
    brelse(bh);
    /***************************************************************/
    if (!i) return 0;
    //////////////////////////////////////////////////////////////////////////
    bh = bread(inode->i_dev, i);
    if (!bh) return 0;
    i = ((unsigned short*) bh->b_data)[block & 511];
    /***************************************************************/
    if (create && !i) {
        if ((i = new_block(inode->i_dev))) {
            ((unsigned short*) (bh->b_data))[block & 511] = i;
            bh->b_dirt = 1;
        }
    }
    brelse(bh);
    /***************************************************************/
    return i;
}
	
/*
 **************************** INTERFACE **************************************
 */

struct m_inode* get_empty_inode(void)
{
    static struct m_inode* last_inode = inode_table;
    struct m_inode* inode;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for ( ; ; ) {
        inode = NULL;
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        for (int i = 0; i < NR_INODE; ++i) {
            if (++last_inode >= inode_table + NR_INODE)
                last_inode = inode_table;   // circular
            /***************************************************************/
            if (!last_inode->i_count) {
                inode = last_inode;
                if (!inode->i_dirt && !inode->i_lock) break;
            }
        }
        /***************************************************************/
        if (!inode) {
#ifdef DEBUG
            printkc("get_empty_inode: i-node in mem has run out!\n");
#endif
            sleep_on(&empty_inode_wait);
            continue;
        }
        /***************************************************************/
        wait_on_inode(inode);
        while (inode->i_dirt) {
            write_inode(inode);
            wait_on_inode(inode);
        }
        /***************************************************************/
        if (!inode->i_count) break;
    } 
    //////////////////////////////////////////////////////////////////////////
    memset(inode, 0, sizeof(*inode));
    inode->i_count = 1;     // mark it as used ahead
    //////////////////////////////////////////////////////////////////////////
    return inode;
}

struct m_inode* get_pipe_inode(void)
{
    struct m_inode* inode = get_empty_inode();
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!inode) return NULL;
    //////////////////////////////////////////////////////////////////////////
    if (!(inode->i_size = get_free_page())) {
#ifdef DEBUG
        printkc("get_pipe_inode: Failed to get a free page!\n");
#endif
        inode->i_count = 0;
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    inode->i_count = 2;	    /* sum of readers/writers */
    PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0;
    inode->i_pipe = 1;
    //////////////////////////////////////////////////////////////////////////
    return inode;
}

// NO lock! Use it cautiously! Make sure that inode won't be released!
void iput(struct m_inode* inode)
{
    if (!inode) return;
    //////////////////////////////////////////////////////////////////////////
    wait_on_inode(inode);
    if (! inode->i_count) panic("iput: trying to free free inode");
    //////////////////////////////////////////////////////////////////////////
    if (inode->i_pipe) {
        wake_up(&inode->i_wait);
        if (-- inode->i_count) return;
        free_page(inode->i_size);
        inode->i_dirt = 0;
        inode->i_pipe = 0;
        inode->i_count = 0;
        wake_up(&empty_inode_wait);
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    if (!inode->i_dev) {    // the device is not valid anymore
        inode->i_count--;
        wake_up(&empty_inode_wait);
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    if (S_ISBLK(inode->i_mode)) {
        sync_dev(inode->i_zone[0]);
        wait_on_inode(inode);
    }
    //////////////////////////////////////////////////////////////////////////
repeat:
    if (inode->i_count > 1) {
        inode->i_count--;
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    if (!inode->i_nlinks) {
        // it is safe to do truncate, since it is an orphan in the filesystem
        truncate(inode);        // TO_READ
        free_inode(inode);
        wake_up(&empty_inode_wait);
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    if (inode->i_dirt) {
        write_inode(inode);	/* we can sleep - so do again */
        wait_on_inode(inode);
        goto repeat;
    }
    //////////////////////////////////////////////////////////////////////////
    inode->i_count--;
    wake_up(&empty_inode_wait);
    return;
}

// no check of vadility of nr! Use it with caution!
struct m_inode* iget(int dev, int nr)
{
    if (!dev) panic("iget with dev==0");
repeat:
    struct m_inode* inode = inode_table;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (inode < NR_INODE + inode_table) {
        if (inode->i_dev != dev || inode->i_num != nr) {
            ++inode;
            continue;
        }
        /***************************************************************/
        wait_on_inode(inode);
        if (inode->i_dev != dev || inode->i_num != nr) {
#ifdef DEBUG
            printkc("iget(): the inode changed during sleep!\n");
#endif
            inode = inode_table;
            continue;
        }
        inode->i_count++;
        ///////////////////////////////////////////////////////////////////////
        // Finally we have inode->idev == dev && inode->inum == nr
        if (inode->i_mount) {
            int i;
            //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
            for (i = 0; ; ++i) {
                if (i == NR_SUPER) {
                    printk("iget(): Mounted inode hasn't got sb!\n");
                    inode->i_mount = 0;
                    return inode;
                }
                /*****************************************************/
                if (super_block[i].s_imount == inode) break;
            }
            /***********************************************************/
            dev = super_block[i].s_dev;
            nr = ROOT_INO;
            inode = inode_table;
            continue;
        }
        /***************************************************************/
        return inode;
    }
    //////////////////////////////////////////////////////////////////////////
    struct m_inode* empty = get_empty_inode();
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
#ifdef DEBUG
    if(!empty) {
        printkc("iget: get_empty_inode() mustn't be NULL!\n");
        panic("iget: get_empty_inode() mustn't be NULL!");
    }
#endif
    inode = empty;
    inode->i_dev = dev;
    inode->i_num = nr;
    read_inode(inode);
    //////////////////////////////////////////////////////////////////////////
    return inode;
}

void sync_inodes(void)
{
    struct m_inode* inode = inode_table;
    for(int i = 0; i < NR_INODE; ++i, ++inode) {
        wait_on_inode(inode);
        if (inode->i_dirt && !inode->i_pipe)
            write_inode(inode);
    }
}

void invalidate_inodes(int dev)
{
    struct m_inode* inode = inode_table;
    for(int i = 0; i < NR_INODE; ++i, ++inode) {
        wait_on_inode(inode);
        if (inode->i_dev == dev) {
            if (inode->i_count) printk("inode in use on removed disk\n");
            /***************************************************************/
            inode->i_dev = inode->i_dirt = 0;
        }
    }
}

int bmap(struct m_inode* inode, int block)
{
	return _bmap(inode, block, 0);
}

int create_block(struct m_inode* inode, int block)
{
	return _bmap(inode, block, 1);
}
	
