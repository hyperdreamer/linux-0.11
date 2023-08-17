#pragma once
/*
 * This file has definitions for some important file table
 * structures etc.
 */

#include <sys/types.h>

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem
 * 2 - /dev/fd
 * 3 - /dev/hd
 * 4 - /dev/ttyx
 * 5 - /dev/tty
 * 6 - /dev/lp
 * 7 - unnamed pipes
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(laddr_t buffer_end);

#define MAJOR(a) (((unsigned int) (a)) >> 8)
#define MINOR(a) ((a)&0xff)

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

#define BLCK_BITS BLOCK_SIZE<<3
#define BLCK_MASK (BLCK_BITS - 1)
#define ZMAP_INDX(znr) ((znr) >> BLOCK_SIZE_BITS + 3)
#define IMAP_INDX ZMAP_INDX

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/sizeof(struct d_inode))
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/sizeof(struct dir_entry))

#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))
#define INC_PIPE(head) \
__asm__("incl %0\n\tandl $4095,%0"::"m" (head))

typedef char buffer_block[BLOCK_SIZE];

struct buffer_head {
    char* b_data;			    /* pointer to data block (1024 bytes) */
    unsigned long b_blocknr;	/* block number */
    unsigned short b_dev;		/* device (0 = free) */
    unsigned char b_uptodate;
    unsigned char b_dirt;		/* 0-clean,1-dirty */
    unsigned char b_count;		/* users using this block */
    unsigned char b_lock;		/* 0 - ok, 1 -locked */
    struct task_struct* b_wait; // the wait queue header, check sleep_on()
    struct buffer_head* b_prev; // for the hash table
    struct buffer_head* b_next; // for the hash table
    struct buffer_head* b_prev_free;// free buffer circular double-linked list
    struct buffer_head* b_next_free;// free buffer circular double-linked list
};

// i-node structure on disks
struct d_inode {
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned long i_size;       // file size
    unsigned long i_time;       // file's time of last modification
    unsigned char i_gid;
    unsigned char i_nlinks;     // for hard link
    unsigned short i_zone[9];   // zone 0~6, indirect, double indirect
};

// i-node structure in memory
struct m_inode {
    // the copy of d_inode
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned long i_size;
    unsigned long i_mtime;
    unsigned char i_gid;
    unsigned char i_nlinks;
    unsigned short i_zone[9];
    /* these are in memory */
    struct task_struct* i_wait;
    unsigned long i_atime;      // last accessed time
    unsigned long i_ctime;      // the i-node's time of last modification
    unsigned short i_dev;       // i-node's device
    unsigned short i_num;       // i-node number in its device
    unsigned short i_count;     // how many time the file is opened
    unsigned char i_lock;       // ??? TO_READ
    unsigned char i_dirt;       // the dirty bit of i-node (for write-through)
    unsigned char i_pipe;
    unsigned char i_mount;      // to specify if a filesystem is mounted on
    unsigned char i_seek;       // for lseek ??? TO_READ
    unsigned char i_update;     // ??? TO_READ
};

struct file {
    unsigned short f_mode;
    unsigned short f_flags;     // status like being opened, ... ???
    unsigned short f_count;     // file descriptor number (index in filp) ???
    struct m_inode* f_inode;
    off_t f_pos;                // off_t is a synoym for long
};

struct super_block {
    unsigned short s_ninodes;       // total nr of i-nodes
    unsigned short s_nzones;        // total nr of zones
    unsigned short s_imap_blocks;   // total nr of blocks for i-node bitmap
    unsigned short s_zmap_blocks;   // total nr of blocks for zone bitmap
    unsigned short s_firstdatazone; // the block nr of the first data zone
    unsigned short s_log_zone_size; // log_2(blocks / zone)
    unsigned long s_max_size;       // max file size
    unsigned short s_magic;         // magic number for super block validation
    /* These are only in memory */
    struct buffer_head* s_imap[I_MAP_SLOTS];// ptr: buffer of i-node bitmap
    struct buffer_head* s_zmap[Z_MAP_SLOTS];// ptr: buffer of zone bitmap
    unsigned short s_dev;           // super block's device nr
    struct m_inode* s_isup;         // the root i-node of the filesystem
    struct m_inode* s_imount;       // the i-node the fs is mounteded to
    unsigned long s_time;           // time of last update
    struct task_struct* s_wait;
    unsigned char s_lock;
    unsigned char s_rd_only;
    unsigned char s_dirt;
};

struct d_super_block {              // super block in the device
    unsigned short s_ninodes;
    unsigned short s_nzones;
    unsigned short s_imap_blocks;
    unsigned short s_zmap_blocks;
    unsigned short s_firstdatazone;
    unsigned short s_log_zone_size;
    unsigned long s_max_size;
    unsigned short s_magic;
};

struct dir_entry {
    unsigned short inode;           // i-node nr (2-byte)
    char name[NAME_LEN];            // file name
};

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern struct buffer_head* start_buffer;
extern int nr_buffers;

extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern int ticks_to_floppy_on(unsigned int dev);
extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);
extern void truncate(struct m_inode* inode);
extern void sync_inodes(void);
extern void invalidate_inodes(int dev);
extern void wait_on(struct m_inode* inode);
extern int bmap(struct m_inode* inode, int block);
extern int create_block(struct m_inode* inode, int block);
extern struct m_inode * namei(const char* pathname);
extern int open_namei(const char* pathname, int flag, int mode,
                      struct m_inode** res_inode);
extern void iput(struct m_inode* inode);
extern struct m_inode* iget(int dev, int nr);
extern struct m_inode* get_empty_inode(void);
extern struct m_inode* get_pipe_inode(void);
extern struct buffer_head* get_hash_table(int dev, int block);
extern struct buffer_head* getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head* bh);
extern void brelse(struct buffer_head* buf);
extern struct buffer_head* bread(int dev, int block);
extern void bread_page(laddr_t addr, int dev, int b[4]);
extern struct buffer_head* breada(int dev, int block, ...);
extern int new_block(int dev);
extern void free_block(int dev, int block);
extern struct m_inode* new_inode(int dev);
extern void free_inode(struct m_inode* inode);
extern int sync_dev(int dev);
extern struct super_block* get_super(int dev);
extern void put_super(int dev);

extern int ROOT_DEV;

extern void mount_root(void);

