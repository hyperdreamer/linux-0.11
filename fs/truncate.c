/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/sched.h>

#include <sys/stat.h>

static void free_ind(int dev, int block)
{
    if (!block) return;
    /***************************************************************/
    struct buffer_head* bh = bread(dev, block);
    if (bh) {
        unsigned short* p = (unsigned short*) bh->b_data;
        for (int i = 0; i < 512; ++i, ++p)
            if (*p) free_block(dev, *p);
        /******************************************************/
        brelse(bh);
    }
    /***************************************************************/
    free_block(dev, block);
}

static void free_dind(int dev, int block)
{
    if (!block) return;
    /***************************************************************/
    struct buffer_head* bh = bread(dev, block);
    if (bh) {
        unsigned short* p = (unsigned short*) bh->b_data;
        for (int i=0; i < 512; ++i, ++p)
            if (*p) free_ind(dev, *p);
        /******************************************************/
        brelse(bh);
    }
    /***************************************************************/
    free_block(dev, block);
}

void truncate(struct m_inode* inode)
{   // only for regular & directory
    if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode))) return;
    //////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < 7; ++i)
        if (inode->i_zone[i]) {
            free_block(inode->i_dev, inode->i_zone[i]);
            inode->i_zone[i] = 0;
        }
    /***************************************************************/
    free_ind(inode->i_dev, inode->i_zone[7]);
    free_dind(inode->i_dev, inode->i_zone[8]);
    /***************************************************************/
    inode->i_zone[7] = inode->i_zone[8] = 0;
    inode->i_size = 0;
    inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_dirt = 1;
}

