/*
 *  linux/fs/file_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <fcntl.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int file_read(struct m_inode* inode, struct file* filp, char* buf, int count)
{
    int left = count;
    if (left <= 0) return 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (left) {
        struct buffer_head* bh;
        int nr = bmap(inode, (filp->f_pos)/BLOCK_SIZE);
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        if (nr) {
            bh = bread(inode->i_dev, nr);
            if (!bh) break;
        }
        else
            bh = NULL;
        /***************************************************************/
        //nr = filp->f_pos % BLOCK_SIZE;
        nr = filp->f_pos & (BLOCK_SIZE-1);
        int chars = MIN(BLOCK_SIZE-nr, left);
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        filp->f_pos += chars;
        left -= chars;
        /***************************************************************/
        if (bh) {
            char* p = nr + bh->b_data;
            while (chars-- > 0) put_fs_byte(*(p++), buf++);
            brelse(bh);
        } 
        else
            while (chars-- > 0) put_fs_byte(0, buf++);
    }
    //////////////////////////////////////////////////////////////////////////
    inode->i_atime = CURRENT_TIME;
    return (count - left) ? (count - left) : -ERROR;
}

int file_write(struct m_inode* inode, struct file* filp, char* buf, int count)
{
    /*
     * ok, append may not work when many processes are writing at the same time
     * but so what. That way leads to madness anyway.
     */
    off_t pos = (filp->f_flags & O_APPEND) ? inode->i_size : filp->f_pos;
    int i = 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (i < count) {
        int block = create_block(inode, pos/BLOCK_SIZE);
        if (!block) break;
        /***************************************************************/
        struct buffer_head* bh = bread(inode->i_dev, block);
        if (!bh) break;
        /***************************************************************/
        //c = pos % BLOCK_SIZE;
        int c = pos & (BLOCK_SIZE-1);
        char* p = c + bh->b_data;
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        c = BLOCK_SIZE - c; // bytes left in the current block
        if (c > count - i) c = count - i;   // c := min(c, count-i)
        /***************************************************************/
        pos += c;
        if (pos > inode->i_size) {
            inode->i_size = pos;
            inode->i_ctime = CURRENT_TIME;  // by Henry
            inode->i_dirt = 1;
        }
        /***************************************************************/
        i += c;
        while (c-->0) *(p++) = get_fs_byte(buf++);
        /***************************************************************/
        bh->b_dirt = 1;
        brelse(bh);
    }
    //////////////////////////////////////////////////////////////////////////
    inode->i_mtime = CURRENT_TIME;
    if (!(filp->f_flags & O_APPEND)) filp->f_pos = pos;
    return (i ? i : -1);    // return nr of bytes written
}

