/*
 *  linux/fs/pipe.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <signal.h>

#include <linux/sched.h>
#include <linux/mm.h>	/* for get_free_page */
#include <asm/segment.h>

extern struct file file_table[NR_FILE];

int read_pipe(struct m_inode* inode, char* buf, int count)
{
    int read = 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (count > 0) {
        int size;
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        while (!(size = PIPE_SIZE(*inode))) { // if pipe empty
            wake_up(&inode->i_wait);
            /* are there any writers? */
            if (inode->i_count != 2) return read;
            sleep_on(&inode->i_wait);
        }
        /*******************************************************/
        int chars = PAGE_SIZE - PIPE_TAIL(*inode);  // boundary for increment
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        if (chars > count) chars = count;
        if (chars > size) chars = size; // chars := min(chars, count, size)
        count -= chars;
        read += chars;
        /*******************************************************/
        size = PIPE_TAIL(*inode);
        PIPE_TAIL(*inode) += chars;
        PIPE_TAIL(*inode) &= (PAGE_SIZE-1);
        while (chars-- > 0) 
            put_fs_byte(((char*) inode->i_size)[size++], buf++);
    }
    //////////////////////////////////////////////////////////////////////////
    wake_up(&inode->i_wait);
    return read;
}
	
int write_pipe(struct m_inode* inode, char* buf, int count)
{
    int written = 0;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (count > 0) {
        int size;
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        while (!(size = (PAGE_SIZE-1) - PIPE_SIZE(*inode))) { // if pipe full
            wake_up(&inode->i_wait);
            if (inode->i_count != 2) { /* no readers */
                current->signal |= (1 << (SIGPIPE-1));
                return (written ? written : -1);
            }
            sleep_on(&inode->i_wait);
        }
        /*******************************************************/
        int chars = PAGE_SIZE - PIPE_HEAD(*inode); // boundary for increment
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        if (chars > count) chars = count;
        if (chars > size) chars = size; // chars := min(chars, count, size)
        count -= chars;
        written += chars;
        /*******************************************************/
        size = PIPE_HEAD(*inode);
        PIPE_HEAD(*inode) += chars;
        PIPE_HEAD(*inode) &= (PAGE_SIZE-1);
        while (chars-- >0)
            ((char*) inode->i_size)[size++] = get_fs_byte(buf++);
    }
    //////////////////////////////////////////////////////////////////////////
    wake_up(&inode->i_wait);
    return written;
}

int sys_pipe(unsigned long* fildes)
{
    struct file* f[2];
    int fd[2];
    int i, j;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    j = 0;
    for(i = 0; j < 2 && i < NR_FILE; ++i)
        if (!file_table[i].f_count) (f[j++] = &file_table[i])->f_count++;
    /***************************************************************/
    if (j == 1) f[0]->f_count = 0;  // clean-up for failed searching
    /***************************************************************/
    if (j < 2) return -1;
    //////////////////////////////////////////////////////////////////////////
    j = 0;
    for(i = 0; j < 2 && i < NR_OPEN; ++i)
        if (!current->filp[i]) {
            current->filp[i] = f[j];
            fd[j] = i;
            ++j;
        }
    /***************************************************************/
    if (j == 1) current->filp[fd[0]] = NULL;    // clean up
    /***************************************************************/
    if (j<2) {
        f[0]->f_count = f[1]->f_count = 0;      // clean up
        return -1;
    }
    //////////////////////////////////////////////////////////////////////////
    struct m_inode* inode = get_pipe_inode();
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!inode) {   // run out of free mem inodes, do some clean-up
        current->filp[fd[0]] = current->filp[fd[1]] = NULL;
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    /***************************************************************/
    f[0]->f_inode = f[1]->f_inode = inode;
    f[0]->f_pos = f[1]->f_pos = 0;
    f[0]->f_mode = 1;		/* read */
    f[1]->f_mode = 2;		/* write */
    put_fs_long(fd[0], &fildes[0]);
    put_fs_long(fd[1], &fildes[1]);
    /***************************************************************/
    return 0;
}

