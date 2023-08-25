/*
 *  linux/fs/buffer.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'buffer.c' implements the buffer-cache functions. Race-conditions have
 * been avoided by NEVER letting a interrupt change a buffer (except for the
 * data, of course), but instead letting the caller do it. NOTE! As interrupts
 * can wake up a caller, some cli-sti sequences are needed to check for
 * sleep-on-calls. These should be extremely quick, though (I hope).
 */

/*
 * NOTE! There is one discordant note here: checking floppies for
 * disk change. This is where it fits best, I think, as it should
 * invalidate changed floppy-disk-caches.
 */

#include <stdarg.h>
 
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/io.h>

extern int end;
extern struct super_block super_block[NR_SUPER];
extern void put_super(int);
extern void invalidate_inodes(int);

struct buffer_head* start_buffer = (struct buffer_head*) &end;
struct buffer_head* hash_table[NR_HASH] = {};
static struct buffer_head* free_list = NULL;
static struct task_struct* buffer_wait = NULL;
int NR_BUFFERS = 0;

static inline void wait_on_buffer(struct buffer_head* bh)
{
	cli();
	while (bh->b_lock) { 
#undef DEBUG
#ifdef DEBUG
        printkc("buffer.c: wait_on_buffer: Oops!\n");
#endif
        sleep_on(&bh->b_wait);
    }
	sti();
}

// no check bh validity! be careful!
static inline void sync_buffer(struct buffer_head* bh)
{
    wait_on_buffer(bh); // no need to lock strictly
    //////////////////////////////////////////////////////////////////////////
    if (bh->b_dirt) ll_rw_block(WRITE, bh);
}

// no check device validity! Be careful
static inline void do_sync(int dev) 
{
    struct buffer_head* bh = start_buffer;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for (int i = 0; i < NR_BUFFERS; ++i, ++bh) {
        if (bh->b_dev != dev) continue;
        //////////////////////////////////////////////////////////////////////
        wait_on_buffer(bh); // no need to lock strictly
        if (bh->b_dev == dev && bh->b_dirt) ll_rw_block(WRITE, bh);
    }
}

static inline void invalidate_buffers(int dev)
{
    struct buffer_head* bh = start_buffer;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for (int i = 0; i < NR_BUFFERS; ++i, ++bh) {
        if (bh->b_dev != dev) continue;
        wait_on_buffer(bh);
        if (bh->b_dev == dev) bh->b_uptodate = bh->b_dirt = 0;
    }
}

#define _hashfn(dev,block) (((unsigned)(dev^block))%NR_HASH)
#define hash(dev,block) hash_table[_hashfn(dev,block)]

static inline void remove_from_queues(struct buffer_head* bh)
{
    /* remove from hash-queue */
	if (bh->b_next) bh->b_next->b_prev = bh->b_prev;
	if (bh->b_prev) bh->b_prev->b_next = bh->b_next;
    /***************************************************************/
	if (hash(bh->b_dev, bh->b_blocknr) == bh)
		hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
    //////////////////////////////////////////////////////////////////////////
    /* remove from free list */
	if (!(bh->b_prev_free) || !(bh->b_next_free))
		panic("Free block list corrupted");
    /***************************************************************/
	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;
	if (free_list == bh) free_list = bh->b_next_free;
}

static inline void insert_into_queues(struct buffer_head* bh)
{
    /* put at end of free list */
	bh->b_next_free = free_list;
	bh->b_prev_free = free_list->b_prev_free;
    /***************************************************************/
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free = bh;
    //////////////////////////////////////////////////////////////////////////
    /* put the buffer in new hash-queue if it has a device */
	bh->b_prev = NULL;
	bh->b_next = NULL;
	if (!bh->b_dev) return;
    /***************************************************************/
	bh->b_next = hash(bh->b_dev, bh->b_blocknr);
	hash(bh->b_dev, bh->b_blocknr) = bh;
	bh->b_next->b_prev = bh;
}

static inline struct buffer_head* find_buffer(int dev, int block)
{		
    struct buffer_head* tmp = hash(dev, block);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (tmp) {
        if (tmp->b_dev == dev && tmp->b_blocknr == block) return tmp;
        /*******************************************************/
        tmp = tmp->b_next;
    }
    //////////////////////////////////////////////////////////////////////////
    return NULL;
}

#define COPYBLK(from, to) \
    __asm__ ("cld\n\t" \
             "rep movsl\n\t" \
             : \
             : \
             "c" (BLOCK_SIZE/4), \
             "S" (from), \
             "D" (to) \
            )

/*
 **************************** INTERFACE **************************************
 */

void buffer_init(laddr_t buffer_end)
{
    struct buffer_head* h = start_buffer;
    void* b =(void*) ((buffer_end == 1<<20) ? (640*1024) : buffer_end);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while ((b -= BLOCK_SIZE) >= ((void*) (h+1))) {
        h->b_dev = 0;
        h->b_dirt = 0;
        h->b_count = 0;
        h->b_lock = 0;
        h->b_uptodate = 0;
        h->b_wait = NULL;
        h->b_next = NULL;
        h->b_prev = NULL;
        h->b_data = (char*) b;
        h->b_prev_free = h-1;
        h->b_next_free = h+1;
        ++h;
        ++NR_BUFFERS;
        /********************************************************/
        // check the boot-up to check why 0xA0000
        if (b == (void*) 0x100000) b = (void*) 0xA0000;
    }
    /***************************************************************/
    --h;
    free_list = start_buffer;
    free_list->b_prev_free = h;
    h->b_next_free = free_list;
    //////////////////////////////////////////////////////////////////////////
    /*
     * for (register int i = 0;i < NR_HASH; ++i) hash_table[i]=NULL;
     */
#ifdef DEBUG
    for (int i = 0;i < NR_HASH; ++i) 
        if (hash_table[i]) { 
            printkc("buffer_init: hash_table is not initialized properly!\n");
            panic("buffer_init: hash_table is not initialized properly!");
        }
#endif
}	

/*
 * Why like this, I hear you say... The reason is race-conditions.
 * As we don't lock buffers (unless we are readint them, that is),
 * something might happen to it while we sleep (ie a read-error
 * will force it bad). This shouldn't really happen currently, but
 * the code is ready.
 */
struct buffer_head* get_hash_table(int dev, int block)
{
    for (;;) {
        struct buffer_head* bh = find_buffer(dev, block);
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        if (!bh) return NULL;
        //////////////////////////////////////////////////////////////////////
        bh->b_count++;  // make sure it won't be freed
        wait_on_buffer(bh);
        /***************************************************************/
        if (bh->b_dev == dev && bh->b_blocknr == block) return bh;
        //////////////////////////////////////////////////////////////////////
        bh->b_count--;  // make sure it can be freed again
    }
}

/*
 * Ok, this is getblk, and it isn't very clear, again to hinder
 * race-conditions. Most of the code is seldom used, (ie repeating),
 * so it should be much more efficient than it looks.
 *
 * The algoritm is changed: hopefully better, and an elusive bug removed.
 */
struct buffer_head* getblk(int dev, int block)
{
#define BADNESS(bh) (((bh)->b_dirt<<1)+(bh)->b_lock)

repeat:
    struct buffer_head* tmp = free_list;
    struct buffer_head* bh = get_hash_table(dev,block);
    if (bh) return bh;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    do {
        if (tmp->b_count) continue;
        /*******************************************************/
        if (!bh || BADNESS(tmp) < BADNESS(bh)) {
            bh = tmp;
            if (!BADNESS(tmp)) break;
        } /* and repeat until we find something good */
    } while ((tmp = tmp->b_next_free) != free_list);
    /***************************************************************/
    if (!bh) {
        sleep_on(&buffer_wait);
        goto repeat;
    }
    /***************************************************************/
    wait_on_buffer(bh);
    if (bh->b_count) goto repeat;
    /***************************************************************/
    while (bh->b_dirt) {
        sync_buffer(bh);
        wait_on_buffer(bh);
        if (bh->b_count) goto repeat;
    }
    //////////////////////////////////////////////////////////////////////////
    /* NOTE!! While we slept waiting for this block, somebody else might */
    /* already have added "this" block to the cache. check it */
    if (find_buffer(dev, block)) goto repeat;
    /* OK, FINALLY we know that this buffer is the only one of it's kind, */
    /* and that it's unused (b_count=0), unlocked (b_lock=0), and clean */
    bh->b_count=1;
    bh->b_dirt=0;
    bh->b_uptodate=0;
    remove_from_queues(bh);
    /***************************************************************/
    bh->b_dev=dev;
    bh->b_blocknr=block;
    insert_into_queues(bh);
    /***************************************************************/
    return bh;
}

void brelse(struct buffer_head* buf)
{
	if (!buf) return;
    //////////////////////////////////////////////////////////////////////////
	wait_on_buffer(buf);
    /***************************************************************/
	if (!(buf->b_count--)) panic("Trying to free free buffer");
    /***************************************************************/
	wake_up(&buffer_wait);
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 */
struct buffer_head* bread(int dev, int block)
{
	struct buffer_head* bh = getblk(dev, block);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
	if (!bh) panic("bread: getblk returned NULL\n");
    //////////////////////////////////////////////////////////////////////////
    /* 
     * Step 1. If the buffer block alreayd exists and is up-to-date,
     * return it directly
     */
	if (bh->b_uptodate) return bh;  // no need to read
    //////////////////////////////////////////////////////////////////////////
    /* Step 2. Read it from the device */
    // the up-to-date flag is set by device driver, if read succeeds
    // Check the end_request()
	ll_rw_block(READ, bh);
    /***************************************************************/
	wait_on_buffer(bh);
	if (bh->b_uptodate) return bh;
    //////////////////////////////////////////////////////////////////////////
    /* 
     * Step 3. if I/O error happends, release the buffer 
     * and return NULL 
     */
	brelse(bh); 
    return NULL;
}

/*
 * Ok, breada can be used as bread, but additionally to mark other
 * blocks for reading as well. End the argument list with a negative
 * number.
 */
struct buffer_head* breada(int dev, int first, ...)
{
    va_list args;
    va_start(args, first);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    struct buffer_head* bh = getblk(dev, first);
    if (!bh) panic("bread: getblk returned NULL\n");
    if (!bh->b_uptodate) ll_rw_block(READ, bh);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while ((first = va_arg(args, int)) >= 0) {
        struct buffer_head* tmp = getblk(dev, first);
        if (tmp) {
            if (!tmp->b_uptodate) ll_rw_block(READA, tmp);
            tmp->b_count--;     // reada: read ahead but not referenced
        }
    }
    va_end(args);
    //////////////////////////////////////////////////////////////////////////
    wait_on_buffer(bh);
    if (bh->b_uptodate) return bh;
    //////////////////////////////////////////////////////////////////////////
    brelse(bh);
    return NULL;
}

/*
 * bread_page reads four buffers into memory at the desired address. It's
 * a function of its own, as there is some speed to be got by reading them
 * all at the same time, not waiting for one to be read, and then another
 * etc.
 */
void bread_page(laddr_t address, int dev, int b[4])
{
    struct buffer_head* bh[4];
    int i;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    for (i = 0; i < 4; ++i)
        if (b[i]) {
            bh[i] = getblk(dev, b[i]);
            if (bh[i] && !bh[i]->b_uptodate) ll_rw_block(READ, bh[i]);
        } 
        else
            bh[i] = NULL;
    //////////////////////////////////////////////////////////////////////////
    for (i = 0; i < 4; ++i, address += BLOCK_SIZE)
        if (bh[i]) {
            wait_on_buffer(bh[i]);
            if (bh[i]->b_uptodate)
                COPYBLK((laddr_t) bh[i]->b_data, address);
            brelse(bh[i]);
        }
}

// no check device validity! Be careful
int sync_dev(int dev)
{
    //Guess: the reason do_sync() twice is to free buffs for sync_inodes() 
    do_sync(dev); 
    sync_inodes();
    do_sync(dev);
    return 0;
}

// system call: sync
int sys_sync(void)
{
	sync_inodes();		/* write out inodes into buffers */
    //////////////////////////////////////////////////////////////////////////
	struct buffer_head* bh = start_buffer;
	for (int i = 0; i < NR_BUFFERS; ++i, ++bh) {
		wait_on_buffer(bh);
		if (bh->b_dirt) ll_rw_block(WRITE, bh);
	}
    //////////////////////////////////////////////////////////////////////////
	return 0;
}

/*
 * This routine checks whether a floppy has been changed, and
 * invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to loose :-)
 *
 * NOTE! Although currently this is only for floppies, the idea is
 * that any additional removable block-device will use this routine,
 * and that mount/open needn't know that floppies/whatever are
 * special.
 */
void check_disk_change(int dev)
{
    if (MAJOR(dev) != 2) return;    // only for floppy
    if (!floppy_change(dev & 0x03)) return;
    //////////////////////////////////////////////////////////////////////////
    for (register int i = 0; i < NR_SUPER; ++i)
        if (super_block[i].s_dev == dev)
            put_super(super_block[i].s_dev);
    //////////////////////////////////////////////////////////////////////////
    invalidate_inodes(dev);
    invalidate_buffers(dev);
}

