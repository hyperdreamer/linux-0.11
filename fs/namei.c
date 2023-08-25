/*
 *  linux/fs/namei.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * Some corrections by tytso.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

#include <string.h> 
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

#define ACC_MODE(x) ("\004\002\006\377"[(x) & O_ACCMODE])

/*
 * comment out this line if you want names > NAME_LEN chars to be
 * truncated. Else they will be disallowed.
 */
/* #define NO_TRUNCATE */

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

static inline void lock_inode(struct m_inode* inode)
{
    cli();
    while (inode->i_lock) {
#ifdef DEBUG
        printkc("namei.c: lock_inode: Oops!\n");
#endif
        sleep_on(&inode->i_wait);
    }
    inode->i_lock=1;
    sti();
}

static inline void unlock_inode(struct m_inode* inode)
{
	if (!inode->i_lock) printk("namei.c: inode not locked\n");
    //////////////////////////////////////////////////////////////////////////
    cli();
    inode->i_lock=0;
    wake_up(&inode->i_wait);
    sti();
}

/*
 *	permission()
 *
 * is used to check for read/write/execute permissions on a file.
 * I don't know if we should look at just the euid or both euid and
 * uid, but that should be easily changed.
 */
inline static bool permission(struct m_inode* inode, int mask)
{
	int mode = inode->i_mode;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    /* special case: not even root can read/write a deleted file */
	if (inode->i_dev && !inode->i_nlinks) return false;
    //////////////////////////////////////////////////////////////////////////
	if (current->euid == inode->i_uid)
		mode >>= 6;
	else if (current->egid == inode->i_gid)
		mode >>= 3;
    //////////////////////////////////////////////////////////////////////////
	if (((mode & mask & 0007) == mask) || suser()) return true;
    //////////////////////////////////////////////////////////////////////////
	return false;
}

/*
 * ok, we cannot use strncmp, as the name is not in our data space.
 * Thus we'll have to use match. No big problem. Match also makes
 * some sanity tests.
 *
 * NOTE! unlike strncmp, match returns 1 for success, 0 for failure.
 */
static int match(int len, const char* name, struct dir_entry* de)
{
	if (!de || !de->inode || len > NAME_LEN) return 0;
	if (len < NAME_LEN && de->name[len]) return 0;

	int same;
    __asm__ ("cld\n\t"
             "fs repe cmpsb\n\t"
             "setz %%al\n\t"
             :
             "=a" (same)
             :
             "0" (0),
             "S" ((long) name),
             "D" ((long) de->name),
             "c" (len)
            );
    return same;
}

/*
 *	add_entry()
 *
 * adds a file entry to the specified directory, using the same
 * semantics as find_entry(). It returns NULL if it failed.
 *
 * NOTE!! The inode part of 'de' is left at 0 - which means you
 * may not sleep between calling this and putting something into
 * the entry, as someone else might have used it while you slept.
 */
static struct buffer_head* add_entry_safely(struct m_inode* dir,
                                            const char* name, 
                                            int namelen, 
                                            inr_t inr)
{
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN)
        return NULL;
#else
    if (namelen > NAME_LEN)
        namelen = NAME_LEN;
#endif
    //////////////////////////////////////////////////////////////////////////
    if (!namelen) return NULL;
    /***************************************************************/
    int block = dir->i_zone[0];
    if (!block) return NULL;
    /***************************************************************/
    /***************************************************************/
    struct buffer_head* bh = bread(dir->i_dev, block);
    if (!bh) return NULL;
    /***************************************************************/
    /***************************************************************/
    int i = 0;
    struct dir_entry* de = (struct dir_entry*) bh->b_data;
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    while (i < I_MAX_DIR_ENTRIES) {
        if ((char*) de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            /***************************************************************/
            block = create_block(dir, i / DIR_ENTRIES_PER_BLOCK);
            if (!block) return NULL;
            /***************************************************************/
            bh = bread(dir->i_dev, block);
            if (!bh) {  // I/O error
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        //////////////////////////////////////////////////////////////////////
        lock_inode(dir);
        if (i * sizeof(struct dir_entry) >= dir->i_size) {
#ifdef DEBUG
            if (de->inode) {
                printkc("add_entry_safely: new entry with non-zero inode!\n");
                panic("add_entry_safely: new entry with non-zero inode!");
            }
#endif
            dir->i_size = (i+1)*sizeof(struct dir_entry);
            dir->i_ctime = CURRENT_TIME;    // essential for size change
            dir->i_dirt = 1;
        }
        /***************************************************************/
        if (!de->inode) {
            de->inode = inr;
            for (i = 0; i < NAME_LEN; ++i)
                de->name[i] = (i < namelen) ? get_fs_byte(name + i) : 0;
            /***************************************************************/
            dir->i_mtime = CURRENT_TIME;
            dir->i_dirt = 1;
            /***************************************************************/
            bh->b_dirt = 1;
            /***************************************************************/
            unlock_inode(dir);
            return bh;
        }
        unlock_inode(dir);
        //////////////////////////////////////////////////////////////////////
        ++de;
        ++i;
    }
    //////////////////////////////////////////////////////////////////////////
    brelse(bh);
    return NULL;
}

/*
 *	find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself (as a parameter - res_dir). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 *
 * This also takes care of the few special cases due to '..'-traversal
 * over a pseudo-root and a mount point.
 */
static struct buffer_head* find_entry(struct m_inode** dir,
                                      const char* name, 
                                      int namelen, 
                                      struct dir_entry** res_dir)
{
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN)
        return NULL;
#else
    if (namelen > NAME_LEN)
        namelen = NAME_LEN;
#endif
    //////////////////////////////////////////////////////////////////////////
    *res_dir = NULL;
    if (!namelen) return NULL;  // check the weird case in get_dir_i
    /***************************************************************/
    /* check for '..', as we might have to do some "magic" for it */
    if (namelen==2 && get_fs_byte(name)=='.' && get_fs_byte(name+1)=='.') {
        /* '..' in a pseudo-root results in a faked '.' (just change namelen) */
        if ((*dir) == current->root)
            namelen = 1;
        else if ((*dir)->i_num == ROOT_INO) {
            /* 
             * '..' over a mount-point results in 'dir' being exchanged for 
             * the mounted directory-inode. NOTE! We set mounted, so that 
             * we can iput the new dir 
             * */
            struct super_block* sb = get_super((*dir)->i_dev);
            if (sb->s_imount) {
                iput(*dir);
                (*dir) = sb->s_imount;
                (*dir)->i_count++;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////

    int entries = (*dir)->i_size / sizeof(struct dir_entry);
    int block = (*dir)->i_zone[0];
    if (!block) return NULL;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    struct buffer_head* bh = bread((*dir)->i_dev, block);
    if (!bh) return NULL;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    int i = 0;
    struct dir_entry* de = (struct dir_entry*) bh->b_data;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    while (i < entries) {
        if ((char*) de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            if (!(block = bmap(*dir, i / DIR_ENTRIES_PER_BLOCK)) ||
                !(bh = bread((*dir)->i_dev, block))) 
            {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry*) bh->b_data;
        }
        /***************************************************************/
        /* system_call: name is in %fs, de in %ds */
        if (match(namelen, name, de)) {
            *res_dir = de;
            return bh;
        }
        /***************************************************************/
        ++de;
        ++i;
    }
    brelse(bh);
    return NULL;
}

/*
 *	get_dir_i(): old name get_dir
 *
 * Getdir traverses the pathname until it hits the topmost directory.
 * It returns NULL on failure.
 */
static struct m_inode* get_dir_i(const char* pathname)
{
    char c;
    struct m_inode* inode;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!current->root || !current->root->i_count) panic("No root inode");
    if (!current->pwd || !current->pwd->i_count) panic("No cwd inode");
    //////////////////////////////////////////////////////////////////////////

    c = get_fs_byte(pathname);
    if (c == '/') { // use abolute path
        inode = current->root;
        ++pathname;
    } 
    else if (c)     // use relative path
        inode = current->pwd;
    else
        return NULL;	/* empty name is bad */
    //////////////////////////////////////////////////////////////////////////

    inode->i_count++;       // later iput() will decrement it
    //////////////////////////////////////////////////////////////////////////

    while (true) {
        if (!S_ISDIR(inode->i_mode) || !permission(inode, MAY_EXEC)) {
            iput(inode);    // only for dirctory with executable permission
            return NULL;
        }
        /***************************************************************/
        const char* thisname = pathname;
        int namelen = 0; 
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        /* A weird case: pathnames end with "//": namelenth := 0 */
        while ((c = get_fs_byte(pathname++)) && c != '/')
            ++namelen;
        // if thisname points to basename returns its father directory's inode
        if (!c) return inode;   // if c == '\0'
        /***************************************************************/
        struct dir_entry* de;
        struct buffer_head* bh = find_entry(&inode, thisname, namelen, &de);
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        if (!bh) {
            iput(inode);
            return NULL;
        }
        /***************************************************************/
        int inr = de->inode;
        int idev = inode->i_dev;
        //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        brelse(bh);
        iput(inode);
        inode = iget(idev, inr);    // iget() will handle mounting
        /***************************************************************/
        if (!inode) return NULL;
    }
}

/*
 *	dir_namei()
 *
 * dir_namei() returns the inode of the directory of the
 * specified name, and the name within that directory.
 */
static struct m_inode* dir_namei(const char* pathname,
                                 int* base_len, 
                                 const char* *basename)
{
    struct m_inode* dir_i = get_dir_i(pathname);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir_i) return NULL;
    /***************************************************************/
    const char* __basename = pathname;
    char c;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    // Note that pathname like "*/" will return basename '\0' 
    // with length 0  
    while ((c = get_fs_byte(pathname++)))
        if (c == '/') __basename = pathname;
    /***************************************************************/
    *base_len = pathname - __basename - 1;  // -1 is crucial
    *basename = __basename;
    /***************************************************************/
    return dir_i;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
static bool is_empty_dir(struct m_inode* inode)
{
    struct buffer_head* bh;
    /***************************************************************/
    int len = inode->i_size / sizeof(struct dir_entry);
    /***************************************************************/
    if (len < 2 || !inode->i_zone[0] ||
        !(bh = bread(inode->i_dev, inode->i_zone[0]))) 
    {
        printk("warning - bad directory on dev %04x\n",inode->i_dev);
        return false;
    }
    /***************************************************************/
    struct dir_entry* de = (struct dir_entry*) bh->b_data;
    /***************************************************************/
    if (de[0].inode != inode->i_num || !de[1].inode || 
        strcmp(".", de[0].name) || strcmp("..", de[1].name)) 
    {
        brelse(bh);     // added by Henry
        printk("warning - bad directory on dev %04x\n",inode->i_dev);
        return false;
    }
    /***************************************************************/
    de += 2;
    int nr = 2;
    int block;
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    while (nr < len) {
        if ((void*) de >= (void*) (bh->b_data + BLOCK_SIZE)) {
            brelse(bh);
            block = bmap(inode, nr / DIR_ENTRIES_PER_BLOCK);
            if (!block) {
                nr += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            /******************************************************/
            bh = bread(inode->i_dev, block);
            if (!bh) return false;  // I/O error happened
            de = (struct dir_entry*) bh->b_data;
        }
        /***************************************************************/
        if (de->inode) {
            brelse(bh);
            return false;
        }
        ++de;
        ++nr;
    }
    brelse(bh);
    return true;
}

/*
 **************************** INTERFACE **************************************
 */

/*
 *	namei()
 *
 * is used by most simple commands to get the inode of a specified name.
 * Open, link etc use their own routines, but this is enough for things
 * like 'chmod' etc.
 */
struct m_inode* namei(const char* pathname)
{
#undef DEBUG
#ifdef DEBUG
    char buf[BLOCK_SIZE];
    char* pbuf = buf;
    const char* copypath = pathname;
    char c;
    while ((c = get_fs_byte(copypath++)))
        *(pbuf++) = c; 
    *pbuf = c;  // '\0'
    printkc("namei(%s)\n", buf);
#endif
    //////////////////////////////////////////////////////////////////////////
    const char* basename;
    int base_len;
    struct m_inode* dir;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    dir = dir_namei(pathname, &base_len, &basename);
    if (!dir) return NULL;
    /* special case: '/usr/' etc */
    if (!base_len) return dir;
    /***************************************************************/
    struct dir_entry* de;
    struct buffer_head* bh;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    bh = find_entry(&dir, basename, base_len, &de);
    if (!bh) {
        iput(dir);
        return NULL;
    }
    //////////////////////////////////////////////////////////////////////////
    int inr = de->inode;
    int dev = dir->i_dev;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    brelse(bh);
    iput(dir);
    /***************************************************************/
    dir = iget(dev, inr);       // iget() will handle mounting
    if (dir) {
        dir->i_atime = CURRENT_TIME;
        dir->i_dirt = 1;
    }
    return dir;
}

/*
 *	open_namei()
 *
 * namei for open - this is in fact almost the whole open-routine.
 */
int open_namei(const char* pathname, 
               int flag, 
               int mode,
               struct m_inode** res_inode)
{
    if ((flag & O_TRUNC) && !(flag & O_ACCMODE)) flag |= O_WRONLY;
    /***************************************************************/
    mode &= 0777 & ~current->umask;
    mode |= I_REGULAR;
    //////////////////////////////////////////////////////////////////////////
    int base_len;
    const char* basename;
    struct m_inode* dir = dir_namei(pathname, &base_len, &basename);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir) return -ENOENT;
    /***************************************************************/
    if (!base_len) {			/* special case: '/usr/' etc */
        if (!(flag & (O_ACCMODE|O_CREAT|O_TRUNC))) {
            *res_inode = dir;
            return 0;
        }
        /***************************************************************/
        iput(dir);
        return -EISDIR;
    }
    /*#######################################################################*/
    /*#######################################################################*/
    struct dir_entry* de;
    struct m_inode* inode;
    struct buffer_head* bh = find_entry(&dir, basename, base_len, &de);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!bh) { // only for new file creation
        if (!(flag & O_CREAT)) {
            iput(dir);
            return -ENOENT;
        }
        /***************************************************************/
        if (!permission(dir, MAY_WRITE)) {
            iput(dir);
            return -EACCES;
        }
        /***************************************************************/
        inode = new_inode(dir->i_dev);
        if (!inode) {
            iput(dir);
            return -ENOSPC;
        }
        /***************************************************************/
        inode->i_uid = current->euid;
        inode->i_mode = mode;
        inode->i_dirt = 1;
        /***************************************************************/
        bh = add_entry_safely(dir, basename, base_len, inode->i_num);
        if (!bh) {
            inode->i_nlinks--;
            iput(inode);
            iput(dir);
            return -ENOSPC;
        }
        /***************************************************************/
        bh->b_dirt = 1;
        brelse(bh);
        iput(dir);
        *res_inode = inode;
        return 0;
    }
    //////////////////////////////////////////////////////////////////////////
    int inr = de->inode;
    int dev = dir->i_dev;
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    brelse(bh);
    iput(dir);
    /***************************************************************/
    if (flag & O_EXCL) return -EEXIST;
    /***************************************************************/
    inode = iget(dev, inr);
    if (!inode) return -EACCES;
    /***************************************************************/
    if ((S_ISDIR(inode->i_mode) && (flag & O_ACCMODE)) ||
        !permission(inode, ACC_MODE(flag)))
    {
        iput(inode);
        return -EPERM;
    }
    /***************************************************************/
    inode->i_atime = CURRENT_TIME;
    if (flag & O_TRUNC) truncate(inode);
    /***************************************************************/
    *res_inode = inode;
    return 0;
}

int sys_mknod(const char* pathname, int mode, int dev)
{
    if (!suser()) return -EPERM;
    /***************************************************************/
    int base_len;
    const char* basename;
    struct m_inode* dir = dir_namei(pathname, &base_len, &basename);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir) return -ENOENT;
    /***************************************************************/
    if (!base_len) {
        iput(dir);
        return -ENOENT;
    }
    /***************************************************************/
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    /***************************************************************/
    struct dir_entry* de;
    struct buffer_head* bh = find_entry(&dir, basename, base_len, &de);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (bh) {
        brelse(bh);
        iput(dir);
        return -EEXIST;
    }
    /***************************************************************/
    struct m_inode* inode = new_inode(dir->i_dev);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!inode) {
        iput(dir);
        return -ENOSPC;
    }
    //////////////////////////////////////////////////////////////////////////

    inode->i_mode = mode;
    if (S_ISBLK(mode) || S_ISCHR(mode)) inode->i_zone[0] = dev;
    /***************************************************************/
    inode->i_mtime = inode->i_ctime = inode->i_atime = CURRENT_TIME;
    inode->i_dirt = 1;
    /***************************************************************/
    bh = add_entry_safely(dir, basename, base_len, inode->i_num);
    if (!bh) {
        inode->i_nlinks = 0;
        iput(inode);
        iput(dir);
        return -ENOSPC;
    }
    /***************************************************************/
    bh->b_dirt = 1;
    brelse(bh);
    /***************************************************************/
    iput(inode);
    iput(dir);
    return 0;
}

int sys_mkdir(const char* pathname, int mode)
{
    if (!suser()) return -EPERM;
    //////////////////////////////////////////////////////////////////////////
    int base_len;
    const char* basename;
    struct m_inode* dir = dir_namei(pathname, &base_len, &basename);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir) return -ENOENT;
    /***************************************************************/
    if (!base_len) {
        iput(dir);
        return -ENOENT;
    }
    /***************************************************************/
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    //////////////////////////////////////////////////////////////////////////
    struct dir_entry* de;
    struct buffer_head* bh = find_entry(&dir, basename, base_len, &de);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (bh) {   // directory already exists
        brelse(bh); // don't forget this
        iput(dir);
        return -EEXIST;
    }
    /*#######################################################################*/
    /*#######################################################################*/
    struct m_inode* inode = new_inode(dir->i_dev);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!inode) {   // disk inodes have run out
        iput(dir);
        return -ENOSPC;
    }
    /***************************************************************/
    if (!(inode->i_zone[0] = new_block(inode->i_dev))) {
        /*******************************************************/
        inode->i_nlinks--;
        iput(inode);
        /*******************************************************/
        iput(dir);
        return -ENOSPC; // disk zones have run out
    }
    //////////////////////////////////////////////////////////////////////////
    struct buffer_head* dir_block = bread(inode->i_dev, inode->i_zone[0]);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir_block) {   // I/O error happened
        free_block(inode->i_dev, inode->i_zone[0]);
        /*******************************************************/
        inode->i_nlinks--;
        iput(inode);
        /*******************************************************/
        iput(dir);
        return -ERROR;
    }
    //////////////////////////////////////////////////////////////////////////
    // for . & ..
    inode->i_size = 32; // size of . & ..
    inode->i_ctime = inode->i_mtime = inode->i_atime = CURRENT_TIME;
    inode->i_dirt = 1;
    //////////////////////////////////////////////////////////////////////////
    // for entry: .
    de = (struct dir_entry*) dir_block->b_data;
    de->inode = inode->i_num;
    strcpy(de->name,".");
    /***************************************************************/
    inode->i_nlinks++;    // due to .
    inode->i_ctime = inode->i_mtime = CURRENT_TIME;
    inode->i_dirt = 1;
#ifdef DEBUG
    if (inode->i_nlinks != 2) {
        free_block(inode->i_dev, inode->i_zone[0]);
        /***************************************************************/
        inode->i_nlinks = 0;
        iput(inode);
        /*******************************************************/
        iput(dir);
        /*******************************************************/
        printkc("sys_mkdir: Wrong i_nlinks %d\n for the new dir", 
                inode->i_nlinks);
        panic("sys_mkdir: Wrong i_nlinks for the new dir!");
    }
#endif
    //////////////////////////////////////////////////////////////////////////
    // for entry: ..
    ++de;
    de->inode = dir->i_num;
    strcpy(de->name, "..");
    /***************************************************************/
    inode->i_mtime = CURRENT_TIME;
    inode->i_dirt = 1;
    /***************************************************************/
    dir_block->b_dirt = 1;
    brelse(dir_block);
    //////////////////////////////////////////////////////////////////////////
    inode->i_mode = I_DIRECTORY | (mode & 0777 & ~current->umask);
    dir->i_ctime = CURRENT_TIME;    // essential for mode change
    inode->i_dirt = 1;
    //////////////////////////////////////////////////////////////////////////
    bh = add_entry_safely(dir, basename, base_len, inode->i_num);
    if (!bh) {
        free_block(inode->i_dev, inode->i_zone[0]);
        /*******************************************************/
        inode->i_nlinks = 0;
        iput(inode);
        /*******************************************************/
        iput(dir);
        return -ENOSPC;
    }
    bh->b_dirt = 1;
    brelse(bh);
    /***************************************************************/
    dir->i_nlinks++;                // for new inode's .. (careful!!!)
    dir->i_ctime = CURRENT_TIME;    // essential for link change
    dir->i_dirt = 1;
    /***************************************************************/
    iput(inode);
    iput(dir);
    return 0;
}

int sys_rmdir(const char* pathname)
{
    if (!suser()) return -EPERM;
    /***************************************************************/
    int base_len;
    const char* basename;
    struct m_inode* dir = dir_namei(pathname, &base_len, &basename);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir) return -ENOENT;
    /***************************************************************/
    if (!base_len) {
        iput(dir);
        return -ENOENT;
    }
    /***************************************************************/
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    /***************************************************************/
    struct dir_entry* de;
    struct buffer_head* bh = find_entry(&dir, basename, base_len, &de);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!bh) {
        iput(dir);
        return -ENOENT;
    }
    /***************************************************************/
    struct m_inode* inode = iget(dir->i_dev, de->inode);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!inode) {
        brelse(bh);
        iput(dir);
        return -EPERM;
    }
    /***************************************************************/
    if ((dir->i_mode & S_ISVTX) && !suser() && inode->i_uid != current->euid) 
    {
        iput(inode);
        brelse(bh);
        iput(dir);
        return -EPERM;
    }
    /***************************************************************/
    if (inode->i_dev != dir->i_dev || inode->i_count > 1) {
        iput(inode);
        brelse(bh);
        iput(dir);
        return -EPERM;
    }
    /***************************************************************/
    if (inode == dir) {	/* we may not delete ".", but "../dir" is ok */
        iput(inode);
        brelse(bh);
        iput(dir);
        return -EPERM;
    }
    /***************************************************************/
    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        brelse(bh);
        iput(dir);
        return -ENOTDIR;
    }
    /***************************************************************/
    if (!is_empty_dir(inode)) {
        iput(inode);
        brelse(bh);
        iput(dir);
        return -ENOTEMPTY;
    }
    /***************************************************************/
    if (inode->i_nlinks != 2)
        printk("empty directory has nlink!=2 (%d)", inode->i_nlinks);
    //////////////////////////////////////////////////////////////////////////

    de->inode = 0;
    bh->b_dirt = 1;
    brelse(bh);
    /***************************************************************/
    inode->i_nlinks = 0;
    inode->i_dirt = 1;
    iput(inode);    // don't free_inode(), you have to truncate() it
    /***************************************************************/
    dir->i_nlinks--;
    dir->i_ctime = dir->i_mtime = CURRENT_TIME;
    dir->i_dirt = 1;
    iput(dir);
    return 0;
}

int sys_unlink(const char* name)
{
    const char * basename;
    int namelen;
    struct m_inode * dir, * inode;
    struct buffer_head * bh;
    struct dir_entry * de;

    if (!(dir = dir_namei(name,&namelen,&basename)))
        return -ENOENT;
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir,MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    bh = find_entry(&dir,basename,namelen,&de);
    if (!bh) {
        iput(dir);
        return -ENOENT;
    }
    if (!(inode = iget(dir->i_dev, de->inode))) {
        iput(dir);
        brelse(bh);
        return -ENOENT;
    }
    if ((dir->i_mode & S_ISVTX) && !suser() &&
        current->euid != inode->i_uid &&
        current->euid != dir->i_uid) {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    if (S_ISDIR(inode->i_mode)) {
        iput(inode);
        iput(dir);
        brelse(bh);
        return -EPERM;
    }
    if (!inode->i_nlinks) {
        printk("Deleting nonexistent file (%04x:%d), %d\n",
               inode->i_dev,inode->i_num,inode->i_nlinks);
        inode->i_nlinks=1;
    }
    de->inode = 0;
    bh->b_dirt = 1;
    brelse(bh);
    inode->i_nlinks--;
    inode->i_dirt = 1;
    inode->i_ctime = CURRENT_TIME;
    iput(inode);
    iput(dir);
    return 0;
}

int sys_link(const char* oldname, const char* newname)
{
    struct m_inode* oldinode = namei(oldname);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!oldinode) return -ENOENT;
    /***************************************************************/
    if (S_ISDIR(oldinode->i_mode)) {
        iput(oldinode);
        return -EPERM;
    }
    //////////////////////////////////////////////////////////////////////////
    int base_len;
    const char* basename;
    struct m_inode* dir = dir_namei(newname, &base_len, &basename);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (!dir) {
        iput(oldinode);
        return -EACCES;
    }
    /***************************************************************/
    if (!base_len) {
        iput(dir);
        iput(oldinode);
        return -EPERM;
    }
    /***************************************************************/
    if (dir->i_dev != oldinode->i_dev) { // must be in the same device
        iput(dir);
        iput(oldinode);
        return -EXDEV;
    }
    /***************************************************************/
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        iput(oldinode);
        return -EACCES;
    }
    /*#######################################################################*/
    /*#######################################################################*/
    struct dir_entry* de;
    struct buffer_head* bh = find_entry(&dir, basename, base_len, &de);
    //;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; 
    if (bh) {
        brelse(bh);
        iput(dir);
        iput(oldinode);
        return -EEXIST;
    }
    //////////////////////////////////////////////////////////////////////////
    bh = add_entry_safely(dir, basename, base_len, oldinode->i_num);
    if (!bh) {
        iput(dir);
        iput(oldinode);
        return -ENOSPC;
    }
    /***************************************************************/
    bh->b_dirt = 1;
    brelse(bh);
    iput(dir);
    //////////////////////////////////////////////////////////////////////////
    oldinode->i_nlinks++;
    oldinode->i_ctime = CURRENT_TIME;
    oldinode->i_dirt = 1;
    iput(oldinode);
    //////////////////////////////////////////////////////////////////////////
    return 0;
}

int sys_symlink()
{
    return -ENOSYS;
}

