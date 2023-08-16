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

int ROOT_DEV = 0; /* this is initialized in init/main.c */

struct super_block super_block[NR_SUPER] = {};
static bool st_lock = false;
static struct task_struct* st_wait = NULL;

static inline void lock_st()
{
    cli();
    while (st_lock) sleep_on(&st_wait);
    st_lock = true;
    sti();
}

static inline void unlock_st()
{
    cli();
    st_lock = false;
    wake_up(&st_wait);
    sti();
}

static inline void lock_super(struct super_block* sb)
{
    cli();
    while (sb->s_lock) sleep_on(&(sb->s_wait));
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
    while (sb->s_lock) sleep_on(&(sb->s_wait));
    sti();
}

// No lock, No check! Use it at your own risk
static inline struct super_block* find_super_directly(int dev)
{
    //if (!dev) return NULL;
    struct super_block* s;
    for (s = &super_block[0]; s < &super_block[NR_SUPER]; ++s)
        if (s->s_dev == dev) return s; 			
    //////////////////////////////////////////////////////////////////////////
    return NULL;
}

// No lock, No check! Use it at your own risk
static inline struct super_block* get_free_super_slot_safely()
{
    lock_st();
    //////////////////////////////////////////////////////////////////////////
    struct super_block* s;
    for (s = &super_block[0]; s < &super_block[NR_SUPER]; ++s)
        if (!s->s_dev) {
            unlock_st();
            return s;
        }
    //////////////////////////////////////////////////////////////////////////
    unlock_st();
    return NULL;
}

struct super_block* get_super_safely(int dev)
{
    if (!dev) return NULL;
    //////////////////////////////////////////////////////////////////////////
    lock_st();
    struct super_block* s = find_super_directly(dev);
    if (s) {
        unlock_st();
        return s;
    }
    unlock_st();
    //////////////////////////////////////////////////////////////////////////
    return NULL;
}

struct super_block* get_super(int dev)
{
    if (!dev) return NULL;
    //////////////////////////////////////////////////////////////////////////
    struct super_block* s = &super_block[0];
    while (s < &super_block[NR_SUPER]) {
        if (s->s_dev == dev) {
            wait_on_super(s);
            if (s->s_dev == dev) return s;
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
	struct super_block * sb;
	/* struct m_inode * inode;*/
	int i;

	if (dev == ROOT_DEV) {
		printk("root diskette changed: prepare for armageddon\n\r");
		return;
	}
	if (!(sb = get_super(dev)))
		return;
	if (sb->s_imount) {
		printk("Mounted disk changed - tssk, tssk\n\r");
		return;
	}
	lock_super(sb);
	sb->s_dev = 0;
	for(i=0;i<I_MAP_SLOTS;i++)
		brelse(sb->s_imap[i]);
	for(i=0;i<Z_MAP_SLOTS;i++)
		brelse(sb->s_zmap[i]);
	unlock_super(sb);
	return;
}

static struct super_block* read_super_safely(int dev)
{
    if (!dev) return NULL;
    //////////////////////////////////////////////////////////////////////////
repeat:
    check_disk_change(dev);     // TO_READ
    //////////////////////////////////////////////////////////////////////////
    struct super_block* s = get_super_safely(dev);
    if (s) return s;
    //////////////////////////////////////////////////////////////////////////
    s = get_free_super_slot_safely();
    //////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
    if (s->s_lock) {
        printkc("Free super block should not be locked!\n");
        printkc("An Interrupt must've happened!\n");
    }
#endif
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    lock_super(s);
    if (s->s_dev) { // it has been taken
        unlock_super(s);
    //////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
        printkc("Go back to get_super_safely() again!\n");
#endif
    //////////////////////////////////////////////////////////////////////////
        goto repeat;
    }
    s->s_dev = dev;
    s->s_isup = NULL;
    s->s_imount = NULL;
    s->s_time = 0;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    //////////////////////////////////////////////////////////////////////////
    // lock super block s & possible hang on bread: possible deadlock?
    // bread() return value is unreliable, Need to check it
    struct buffer_head* bh = bread(dev, 1);
    if (!bh) {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    *((struct d_super_block *) s) = *((struct d_super_block *) bh->b_data);
    brelse(bh);
    //////////////////////////////////////////////////////////////////////////
    if (s->s_magic != SUPER_MAGIC) {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    register int i;
    for (i = 0; i < I_MAP_SLOTS; ++i) s->s_imap[i] = NULL;
    for (i = 0; i < Z_MAP_SLOTS; ++i) s->s_zmap[i] = NULL;
    //////////////////////////////////////////////////////////////////////////
    int block = 2; // boot block & super block
    for (i = 0; i < s->s_imap_blocks; ++i)
        if ((s->s_imap[i] = bread(dev, block)))
            ++block;
        else
            break;
    //////////////////////////////////////////////////////////////////////////
    for (i = 0 ; i < s->s_zmap_blocks; i++)
        if ((s->s_zmap[i] = bread(dev, block)))
            ++block;
        else
            break;
    //////////////////////////////////////////////////////////////////////////
    // something wrong with the super block read
    if (block != 2 + s->s_imap_blocks + s->s_zmap_blocks) {
        for(i = 0; i < I_MAP_SLOTS; ++i) brelse(s->s_imap[i]);
        for(i = 0; i < Z_MAP_SLOTS; ++i) brelse(s->s_zmap[i]);
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    s->s_imap[0]->b_data[0] |= 1;   // make sure i-node 0 is not used
    s->s_zmap[0]->b_data[0] |= 1;   // make sure zone 0 is used by the root
    unlock_super(s);
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    return s;
}

static struct super_block* read_super(int dev)
{
    if (!dev) return NULL;
    //////////////////////////////////////////////////////////////////////////
repeat:
    check_disk_change(dev);     // TO_READ
    //////////////////////////////////////////////////////////////////////////
    struct super_block* s = get_super(dev);
    if (s) return s;
    //////////////////////////////////////////////////////////////////////////
    for (s = &super_block[0]; ; ++s) { // try to find a free slot
        if (s >= &super_block[NR_SUPER]) return NULL;   // if no free slot
        if (!s->s_dev) break;
    }
    //////////////////////////////////////////////////////////////////////////
    if (s->s_lock) panic("Free super block should not be locked!\n");
    lock_super(s);
    s->s_dev = dev;
    s->s_isup = NULL;
    s->s_imount = NULL;
    s->s_time = 0;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    struct buffer_head* bh = bread(dev, 1);
    if (!bh) {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    *((struct d_super_block *) s) =
        *((struct d_super_block *) bh->b_data);
    brelse(bh);
    //////////////////////////////////////////////////////////////////////////
    if (s->s_magic != SUPER_MAGIC) {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    register int i;
    for (i = 0; i < I_MAP_SLOTS; ++i) s->s_imap[i] = NULL;
    for (i = 0; i < Z_MAP_SLOTS; ++i) s->s_zmap[i] = NULL;
    //////////////////////////////////////////////////////////////////////////
    int block = 2; // boot block & super block
    for (i = 0; i < s->s_imap_blocks; ++i)
        if ((s->s_imap[i] = bread(dev, block)))
            ++block;
        else
            break;
    //////////////////////////////////////////////////////////////////////////
    for (i = 0 ; i < s->s_zmap_blocks; i++)
        if ((s->s_zmap[i] = bread(dev, block)))
            ++block;
        else
            break;
    //////////////////////////////////////////////////////////////////////////
    // something wrong with the super block read
    if (block != 2 + s->s_imap_blocks + s->s_zmap_blocks) {
        for(i = 0; i < I_MAP_SLOTS; ++i) brelse(s->s_imap[i]);
        for(i = 0; i < Z_MAP_SLOTS; ++i) brelse(s->s_zmap[i]);
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    s->s_imap[0]->b_data[0] |= 1;   // make sure i-node 0 is not used
    s->s_zmap[0]->b_data[0] |= 1;   // make sure zone 0 is used by the root
    unlock_super(s);
    //////////////////////////////////////////////////////////////////////////
    return s;
}

int sys_umount(char * dev_name)
{
	struct m_inode * inode;
	struct super_block * sb;
	int dev;

	if (!(inode=namei(dev_name)))
		return -ENOENT;
	dev = inode->i_zone[0];
	if (!S_ISBLK(inode->i_mode)) {
		iput(inode);
		return -ENOTBLK;
	}
	iput(inode);
	if (dev==ROOT_DEV)
		return -EBUSY;
	if (!(sb=get_super(dev)) || !(sb->s_imount))
		return -ENOENT;
	if (!sb->s_imount->i_mount)
		printk("Mounted inode has i_mount=0\n");
	for (inode=inode_table+0 ; inode<inode_table+NR_INODE ; inode++)
		if (inode->i_dev==dev && inode->i_count)
				return -EBUSY;
	sb->s_imount->i_mount=0;
	iput(sb->s_imount);
	sb->s_imount = NULL;
	iput(sb->s_isup);
	sb->s_isup = NULL;
	put_super(dev);
	sync_dev(dev);
	return 0;
}

int sys_mount(char * dev_name, char * dir_name, int rw_flag)
{
	struct m_inode * dev_i, * dir_i;
	struct super_block * sb;
	int dev;

	if (!(dev_i=namei(dev_name)))
		return -ENOENT;
	dev = dev_i->i_zone[0];
	if (!S_ISBLK(dev_i->i_mode)) {
		iput(dev_i);
		return -EPERM;
	}
	iput(dev_i);
	if (!(dir_i=namei(dir_name)))
		return -ENOENT;
	if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO) {
		iput(dir_i);
		return -EBUSY;
	}
	if (!S_ISDIR(dir_i->i_mode)) {
		iput(dir_i);
		return -EPERM;
	}
	if (!(sb=read_super(dev))) {
		iput(dir_i);
		return -EBUSY;
	}
	if (sb->s_imount) {
		iput(dir_i);
		return -EBUSY;
	}
	if (dir_i->i_mount) {
		iput(dir_i);
		return -EPERM;
	}
	sb->s_imount=dir_i;
	dir_i->i_mount=1;
	dir_i->i_dirt=1;		/* NOTE! we don't iput(dir_i) */
	return 0;			/* we do that in umount */
}

/* set_bit uses setb, as gas doesn't recognize setc */
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

void mount_root(void)
{
    int i;
    extern void wait_for_keypress(void);
    //////////////////////////////////////////////////////////////////////////
    if (32 != sizeof (struct d_inode)) panic("bad i-node size");
    //////////////////////////////////////////////////////////////////////////
        for(i = 0;i < NR_FILE; ++i) file_table[i].f_count=0;
    if (MAJOR(ROOT_DEV) == 2) {
        printk("Insert root floppy and press ENTER");
        wait_for_keypress();        // TO_READ
    }
    //////////////////////////////////////////////////////////////////////////
    /* already inintialized
    for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; ++p) {
        p->s_dev = 0;
        p->s_lock = 0;
        p->s_wait = NULL;
    }
    */
    //////////////////////////////////////////////////////////////////////////
    //struct super_block* p = read_super(ROOT_DEV);
    struct super_block* p = read_super_safely(ROOT_DEV);
    if (!p) panic("Unable to mount root");
    //////////////////////////////////////////////////////////////////////////
    struct m_inode* mi = iget(ROOT_DEV, ROOT_INO);
    if (!mi) panic("Unable to read root i-node");
    //////////////////////////////////////////////////////////////////////////
    mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
    p->s_isup = p->s_imount = mi;   // +1
    current->pwd = mi;              // +1
    current->root = mi;             // +1
    //////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
    int free = 0;
    i = p->s_nzones;    // zone nr starts at 0
    while (--i >= 0)
        if (!bit_set(i & BLCK_MASK, p->s_zmap[ZMAP_INDX(i)]->b_data))
            ++free;
    printk("%d/%d free blocks\n\r", free, p->s_nzones);
    //////////////////////////////////////////////////////////////////////////
    free = 0;
    i= p->s_ninodes + 1;    // i-node nr starts at 1
    while (--i >= 0)
        if (!bit_set(i & BLCK_MASK, p->s_imap[IMAP_INDX(i)]->b_data))
            ++free;
    printk("%d/%d free inodes\n\r", free, p->s_ninodes);
#endif
}
