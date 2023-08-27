/*
 *  linux/fs/block_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

int block_write(int dev, long* pos, char* buf, int count)
{
    int block = *pos >> BLOCK_SIZE_BITS;
    int offset = *pos & (BLOCK_SIZE-1);
    int written = 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    while (count > 0) {
        int chars = BLOCK_SIZE - offset;    // bytes left in current block
        if (chars > count) chars = count;
        /***************************************************************/
        struct buffer_head* bh; 
        if (chars == BLOCK_SIZE)
            bh = getblk(dev, block);
        else
            bh = breada(dev, block, block+1, block+2, -1);
        /***************************************************************/
        if (!bh) return (written ? written : -EIO);
        /***************************************************************/
        *pos += chars;
        written += chars;
        count -= chars;
        /***************************************************************/
        char* p = offset + bh->b_data;
        while (chars-- > 0) *(p++) = get_fs_byte(buf++);
        bh->b_dirt = 1;
        brelse(bh);
        /***************************************************************/
        offset = 0;
        ++block;
    }
    //////////////////////////////////////////////////////////////////////////
    return written;
}

int block_read(int dev, unsigned long* pos, char* buf, int count)
{
    int block = *pos >> BLOCK_SIZE_BITS;
    int offset = *pos & (BLOCK_SIZE-1);
    int read = 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    while (count > 0) {
        int chars = BLOCK_SIZE - offset;    // bytes left in current block
        if (chars > count) chars = count;
        /***************************************************************/
        struct buffer_head* bh = breada(dev, block, block+1, block+2, -1);
        if (!bh) return (read ? read : -EIO);
        /***************************************************************/
        *pos += chars;
        read += chars;
        count -= chars;
        /***************************************************************/
        char* p = offset + bh->b_data;
        while (chars-- > 0) put_fs_byte(*(p++), buf++);
        brelse(bh);
        /***************************************************************/
        offset = 0;
        ++block;
    }
    //////////////////////////////////////////////////////////////////////////
    return read;
}
