/*
 *  linux/fs/super.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * super.c contains code to handle the super-block tables.
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

#include <asm/segment.h>

/* bit_set uses setb, as gas doesn't recognize setc */
#define bit_set(bitnr, addr) \
    ({ \
        int __res ; \
        __asm__ ("btl %2, %3\n\t" \
                 "setb %%al" \
                 : \
                 "=a" (__res) \
                 : \
                 "a" (0), \
                 "r" (bitnr), \
                 "m" (*(addr)) \
                ); \
        __res; \
    })

int ROOT_DEV = 0; /* this is initialized in init/main.c */
struct super_block super_block[NR_SUPER] = {};

static inline void lock_super(struct super_block* sb)
{
    cli();
    while (sb->s_lock) {
#ifdef DEBUG
        printkc("super.c: lock_super: Oops!\n");
#endif
        sleep_on(&(sb->s_wait));
    }
    sb->s_lock = 1;
    sti();
}

static inline void unlock_super(struct super_block* sb)
{
    cli();
    sb->s_lock = 0;
    wake_up(&(sb->s_wait));
    sti();
}

static inline void wait_on_super(struct super_block* sb)
{
    cli();
    while (sb->s_lock) {
#ifdef DEBUG
        printkc("super.c: wait_on_super: Oops!\n");
#endif
        sleep_on(&(sb->s_wait));
    }
    sti();
}

static inline struct super_block* get_free_super()
{
    struct super_block* s;
    for (s = &super_block[0]; s < &super_block[NR_SUPER]; ++s)
        if (!s->s_dev) return s;
    //////////////////////////////////////////////////////////////////////////
    return NULL;
}

static struct super_block* read_super(int dev)
{
    if (!dev) return NULL;
    /***************************************************************/
    check_disk_change(dev);     // TO_READ
    /***************************************************************/
    struct super_block* s = get_super(dev);
    if (s) return s;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    s = get_free_super();
    /***************************************************************/
    s->s_dev = dev;
    s->s_isup = NULL;
    s->s_imount = NULL;
    s->s_time = 0;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    /***************************************************************/
    lock_super(s);
    struct buffer_head* bh = bread(dev, 1);
    if (!bh) {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    /***************************************************************/
    *((struct d_super_block *) s) = *((struct d_super_block *) bh->b_data);
    brelse(bh);
    /***************************************************************/
    if (s->s_magic != SUPER_MAGIC) {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    int i;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for (i = 0; i < I_MAP_SLOTS; ++i) s->s_imap[i] = NULL;
    for (i = 0; i < Z_MAP_SLOTS; ++i) s->s_zmap[i] = NULL;
    /***************************************************************/
    int block = 2; // boot block & super block
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for (i = 0; i < s->s_imap_blocks; ++i)
        if ((s->s_imap[i] = bread(dev, block)))
            ++block;
        else
            break;
    /***************************************************************/
    for (i = 0 ; i < s->s_zmap_blocks; i++)
        if ((s->s_zmap[i] = bread(dev, block)))
            ++block;
        else
            break;
    /***************************************************************/
    // something wrong with the super block read
    if (block != 2 + s->s_imap_blocks + s->s_zmap_blocks) {
        for(i = 0; i < s->s_imap_blocks; ++i) brelse(s->s_imap[i]);
        for(i = 0; i < s->s_zmap_blocks; ++i) brelse(s->s_zmap[i]);
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    s->s_imap[0]->b_data[0] |= 1;   // make sure i-node 0 is not used
    s->s_zmap[0]->b_data[0] |= 1;   // make sure zone 0 is used by the root
    unlock_super(s);
    /***************************************************************/
#ifdef DEBUG
    printkc("\nDev: %#x, IMAP blocks: %d, ZMAP blocks: %d, "
            "The zone nr of data_zone[1]: %d\n",
            s->s_dev, s->s_imap_blocks, s->s_zmap_blocks,
            s->s_firstdatazone);
    /***************************************************************/
    printkc("\nThe zone nr of data_zone[0] by Calculation: %d\n\n",
            2 + s->s_imap_blocks + s->s_zmap_blocks +
            s->s_ninodes / INODES_PER_BLOCK);
#endif
    return s;
}

/*
 **************************** INTERFACE **************************************
 */

struct super_block* get_super(int dev)
{
    if (!dev) return NULL;
    //////////////////////////////////////////////////////////////////////////
    struct super_block* s = &super_block[0];
    while (s < &super_block[NR_SUPER]) {
        if (s->s_dev == dev) {
            wait_on_super(s);
            if (s->s_dev == dev) return s;
            /***************************************************************/
            s = &super_block[0];
            continue;
        } 			
        ++s;
    }
    //////////////////////////////////////////////////////////////////////////
    return NULL;
}

void put_super(int dev)
{
    if (dev == ROOT_DEV) {
        printk("root diskette changed: prepare for armageddon\n\r");
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    struct super_block* sb = get_super(dev);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!sb) return;
    if (sb->s_imount) {
        printk("Mounted disk changed - tssk, tssk\n\r");
        return;
    }
    //////////////////////////////////////////////////////////////////////////
    lock_super(sb);
    sb->s_dev = 0;
    int i;
    for(i = 0; i < sb->s_imap_blocks; ++i) brelse(sb->s_imap[i]);
    for(i = 0; i < sb->s_zmap_blocks; ++i) brelse(sb->s_zmap[i]);
    unlock_super(sb);
    //////////////////////////////////////////////////////////////////////////
    return;
}

int sys_mount(char* dev_name, char* dir_name, int rw_flag)
{
    struct m_inode* dev_i = namei(dev_name);    // increment i_count
    if (!dev_i) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!S_ISBLK(dev_i->i_mode)) {
        iput(dev_i);        // decrement i_count
        return -EPERM;
    }
    /***************************************************************/
    int dev = dev_i->i_zone[0];
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    iput(dev_i);            // decrement i_count
    //////////////////////////////////////////////////////////////////////////
    struct m_inode* dir_i = namei(dir_name);
    if (!dir_i) return -ENOENT;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO) {
        iput(dir_i);
#ifdef DEBUG
        printkc("sys_mount: Busy 1\n");
#endif
        return -EBUSY;
    }
    /***************************************************************/
    if (!S_ISDIR(dir_i->i_mode)) {
        iput(dir_i);
        return -EPERM;
    }
    //////////////////////////////////////////////////////////////////////////
    struct super_block* sb = read_super(dev);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!sb) {
        iput(dir_i);
#ifdef DEBUG
        printkc("sys_mount: Busy 2\n");
#endif
        return -EBUSY;
    }
    /***************************************************************/
    if (sb->s_imount) {
        iput(dir_i);
#ifdef DEBUG
        printkc("sys_mount: Busy 3\n");
#endif
        return -EBUSY;
    }
    /***************************************************************/
    if (dir_i->i_mount) {
        iput(dir_i);
        return -EPERM;
    }
    /***************************************************************/
    dev_i = iget(dev, ROOT_INO);
    sb->s_isup = dev_i;
    sb->s_imount = dir_i;
    dir_i->i_mount = 1;
    dir_i->i_dirt = 1;		
    /***************************************************************/
    // NOTE! we don't iput(dir_i) & input(dev_i), they are done by 
    // sys_umount
    return 0;			    
}

int sys_umount(char* dev_name)
{
	struct m_inode* inode = namei(dev_name);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if (!inode) return -ENOENT;
    /***************************************************************/
	if (!S_ISBLK(inode->i_mode) && !S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTBLK;
	}
    //////////////////////////////////////////////////////////////////////////
    int dev;
	struct super_block* sb;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (S_ISBLK(inode->i_mode)) {
        dev = inode->i_zone[0];
        sb = get_super(dev);
        /***************************************************************/
        if (!sb || !(sb->s_imount)) {
            iput(inode);
            return -ENOENT;
        }
    }
    else {
        char name[BLOCK_SIZE];
        char* pstr = name;
        char c;
        while ((c = get_fs_byte(dev_name++)))
            *(pstr++) = c; 
        *pstr = c;  // '\0'
        /***************************************************************/
        sb = get_super(inode->i_dev);
        /***************************************************************/
        if (!sb || !(sb->s_imount)) {
            iput(inode);
            return -ENOENT;
        }
        /***************************************************************/
        if (inode != sb->s_isup) {
            iput(inode);
            printk("umount: %s: not mounted\n", name);
            return -ENOTBLK;
        }
        /***************************************************************/
        dev = sb->s_dev;
        // very tricky! Check what namei() has done!
        iput(sb->s_imount); 
    }
    /***************************************************************/
	iput(inode);
    /***************************************************************/
	if (dev == ROOT_DEV) {
        put_super(dev);
        return -EBUSY;
    }
    //////////////////////////////////////////////////////////////////////////
	if (! sb->s_imount->i_mount) printk("Mounted inode has i_mount=0\n");
    /***************************************************************/
    // since we set sb->s_isup, so here inode->i_count == 1
	for (inode = inode_table; inode < inode_table+NR_INODE; ++inode)
		if (inode->i_dev == dev) {
            if (inode->i_num == ROOT_INO) { 
                if (inode->i_count > 1) return -EBUSY;
            }
            else if (inode->i_count)
                return -EBUSY;
        }
    /***************************************************************/
	sb->s_imount->i_mount = 0;
	iput(sb->s_imount);
	sb->s_imount = NULL;
	iput(sb->s_isup);
	sb->s_isup = NULL;
	put_super(dev);
	sync_dev(dev);
	return 0;
}

void mount_root(void)
{
    int i;
    extern void wait_for_keypress(void);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (32 != sizeof(struct d_inode)) panic("bad i-node size");
    //////////////////////////////////////////////////////////////////////////
    for(i = 0; i < NR_FILE; ++i) file_table[i].f_count = 0;
    if (MAJOR(ROOT_DEV) == 2) {
        printk("Insert root floppy and press ENTER");
        wait_for_keypress();        // TO_READ
    }
    /***************************************************************/
    /* already inintialized
    for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; ++p) {
        p->s_dev = 0;
        p->s_lock = 0;
        p->s_wait = NULL;
    }
    */
#ifdef DEBUG
    for (i = 0; i < NR_SUPER; ++i) {
        if (super_block[i].s_dev || super_block[i].s_lock ||
            super_block[i].s_wait)
        {
            printkc("Super block table is not initialized!\n");
            panic("Super block table is not initialized!");
        }
    }
#endif
    //////////////////////////////////////////////////////////////////////////
    struct super_block* p = read_super(ROOT_DEV);
    if (!p) panic("Unable to mount root");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    struct m_inode* mi = iget(ROOT_DEV, ROOT_INO);
    if (!mi) panic("Unable to read root i-node");
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
    p->s_isup = p->s_imount = mi;   // +1
    current->pwd = mi;              // +1
    current->root = mi;             // +1
    //////////////////////////////////////////////////////////////////////////
//#undef DEBUG
#ifdef DEBUG
    int free = 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    i = p->s_nzones + 1;    // zone 0 is used by the root of the fs
    while (--i >= 0)
        if (!bit_set(i & BLCK_MASK, p->s_zmap[ZMAP_INDX(i)]->b_data))
            ++free;
    printk("%d/%d free blocks\n\r", free, p->s_nzones);
    /***************************************************************/
    free = 0;
    i= p->s_ninodes + 1;    // i-node 0 is preserved and is never used
    while (--i >= 0)
        if (!bit_set(i & BLCK_MASK, p->s_imap[IMAP_INDX(i)]->b_data))
            ++free;
    printk("%d/%d free inodes\n\r", free, p->s_ninodes);
#endif
}
