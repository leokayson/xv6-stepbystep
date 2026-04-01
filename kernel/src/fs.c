#include "fs.h"
#include "defs.h"
#include "file.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "string.h"
#include "bio.h"
#include "log.h"
#include "virtio.h"
#include "printf.h"

struct {
	struct spinlock lock;
	struct inode	inode[NINODE];
} itable;

// there should be one superblock per disk device, but we run with only one device
struct superblock sb;

// fs.c
void fsinit(int dev);

// blocks 的分配和释放
static void readsb(int dev, struct superblock *sb);
static void bzero(int dev, int bno);
static uint balloc(uint dev);
static void bfree(int dev, uint b);
static uint bmap(struct inode *ip, uint bn);

// inodes 的分配、封信、释放
struct inode		*ialloc(uint dev, inodetype type);
struct inode		*idup(struct inode *ip);	// TODO: what's dup meaning here?
static struct inode *iget(uint dev, uint inum);
void				 iinit();
void				 iupdate(struct inode *ip);
void				 ilock(struct inode *ip);
void				 iunlock(struct inode *ip);
void				 iput(struct inode *ip);
void				 iunlockput(struct inode *ip);

/*
    在xv6中没有封装 vfs 层，所以在后面系统调用api中，直接调用针对inode操作的函数
        1. readsb 直接读取超级快，通过操作缓存api来实现；
        2. readi 将偏移，转化为对应的块号。将读取的内容，拷贝到dst
		3. writei 写入到内存，未写入到磁盘
		4. stati 获取inode状态
		5. 目录操作 调用namex，dirlookup（将内容强转）
*/
// inode 内容的获取、写入
void itrunc(struct inode *ip);
void stati(struct inode *ip, struct stat *st);
int	 readi(struct inode *ip, bool_t is_user_dst, uint64 dst, uint off, uint n);
int	 writei(struct inode *ip, bool_t is_user_src, uint64 src, uint off, uint n);

// directories 内容的获取、写入
static char	 *skipelem(char *path, char *name);
int			  namecmp(const char *s, const char *t);
struct inode *dirlookup(struct inode *dp, char *name, uint *poff);
int			  dirlink(struct inode *dp, char *name, uint inum);

// paths 内容的获取、写入
static struct inode *namex(char *path, bool_t nameiparent, char *name);
struct inode		*namei(char *path);
struct inode		*nameiparent(char *path, char *name);

/*
    文件系统，分5层
        1. block，磁盘上的一个个独立的块；
        2. log，日志层，用于一次更新多个块时，防止崩溃导致数据不一致；
        3. file，针对inode的分配、读、写、元数据的操作；
        4. directory，目录层，特殊的inode；
        5. names，实现类似于/usr/bin/ls这样的目录结构，便于上层使用
*/

// === block 操作相关函数 ===
static uint balloc(uint dev)
{
	int			b, bi, m;
	struct buf *bp;

	bp = 0;

	for (b = 0; b < sb.size; b += BPB) { // 1024 * 8, 1个块中包含1024 * 8个块状态
		bp = bread(dev, BBLOCK(b, sb));
		for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
			m = 1 << (bi % 8); // 1 个bitmap中的编译量
			if ((bp->data[bi / 8] & m) == 0) { // 此bit表示块是否被使用
				bp->data[bi / 8] |= m; // 标记被使用
				log_write(bp);
				brelse(bp);
				bzero(dev, b + bi); // 将块指向数据块，清零
				return b + bi; // 1024 * 8 + （1个块中的偏移）
			}
		}
	}
	panic("balloc: out of blocks");
	return 0;
}

static void bfree(int dev, uint b)
{
	int			bi, m;
	struct buf *bp;

	bp = bread(dev, BBLOCK(b, sb));
	bi = b % BPB;
	m  = 1 << (bi % 8);
	if ((bp->data[bi / 8] & m) == 0) {
		panic("freeing free block");
	}

	bp->data[bi / 8] &= ~m;
	log_write(bp);
	brelse(bp);
}

static void bzero(int dev, int bno)
{
	struct buf *bp;

	bp = bread(dev, bno);
	memset(bp->data, 0, BSIZE);
	log_write(bp);
	brelse(bp);
}

static void readsb(int dev, struct superblock *sb)
{
	struct buf *bp;
	bp = bread(dev, 1);
	memmove(sb, bp->data, sizeof(struct superblock));
	brelse(bp);
}

// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
// returns 0 if out of disk space.
static uint bmap(struct inode *ip, uint bn)
{
	uint		addr, *a;
	struct buf *bp;

	if (bn < NDIRECT) {
		if ((addr = ip->addrs[bn]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0)
				return 0;
			ip->addrs[bn] = addr;
		}
		return addr;
	}
	bn -= NDIRECT;

	if (bn < NINDIRECT) {
		// Load indirect block, allocating if necessary.
		if ((addr = ip->addrs[NDIRECT]) == 0) {
			addr = balloc(ip->dev);
			if (addr == 0)
				return 0;
			ip->addrs[NDIRECT] = addr;
		}
		bp = bread(ip->dev, addr);
		a  = (uint *)bp->data;
		if ((addr = a[bn]) == 0) {
			addr = balloc(ip->dev);
			if (addr) {
				a[bn] = addr;
				log_write(bp);
			}
		}
		brelse(bp);
		return addr;
	}

	panic("bmap: out of range");
}

// === fs 操作相关函数 ===
void fsinit(int dev)
{
	readsb(dev, &sb);
	if (sb.magic != FSMAGIC) {
		panic("invalid fs");
	}
	initlog(dev, &sb);
}

// === inode 操作相关函数(分配、封信、释放) ===
/*
    1. 1个inode表示1个未包含文件名的文件或目录；
    2. 磁盘上的inode结构体包括：文件类型、文件大小、文件被引用的数据、内容
    3. inode 依次在磁盘中连续存放，起始位置sb.inodestart，所以每个inode都有1个类似数据的索引编号；

    操作系统如何处理inode呢？
        1.  内核将所有正在使用的inode存放在1个表中；
            编号相同的inode只在内存中占用同一个位置；
        2. 相关函数
            分配：ialloc分配，iput释放
            引用：iget创建或增加其在表中的引用，iput减少其在表中的引用
            有效：ip->is_invalid表示有效，ilock从磁盘上读取数据，iput置is_valid为0
    通常使用方式：
        ip = iget(dev, inum);
		begin_op();
        ilock(ip);
        ...
		iunlock(ip);
		end_op();
*/
void iinit()
{
	int i = 0;

	initlock(&itable.lock, "itable");
	for (i = 0; i < NINODE; i++) {
		initsleeplock(&itable.inode[i].lock, "inode");
	}
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode *iget(uint dev, uint inum)
{
	struct inode *ip, *empty;

	acquire(&itable.lock);

	// Is the inode already in the table?
	empty = NULL;
	for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
		if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
			ip->ref++;
			release(&itable.lock);
			return ip;
		}
		if (empty == NULL && ip->ref == 0) { // Remember empty slot.
			empty = ip;
		}
	}

	// Recycle an inode entry.
	if (empty == NULL) {
		panic("iget: no inodes");
	}

	ip			 = empty;
	ip->dev		 = dev;
	ip->inum	 = inum;
	ip->ref		 = 1;
	ip->is_valid = FALSE;
	release(&itable.lock);

	return ip;
}

// Allocate an inode on device dev.
// Mark it as allocated by  giving it type type.
// Returns an unlocked but allocated and referenced inode,
// or NULL if there is no free inode.
struct inode *ialloc(uint dev, inodetype type)
{
	int			   inum;
	struct buf	  *bp;
	struct dinode *dip;

	for (inum = 1; inum < sb.ninodes; inum++) {
		bp	= bread(dev, IBLOCK(inum, sb));
		dip = (struct dinode *)bp->data + inum % IPB;
		if (dip->type == I_NONE) { // a free inode
			memset(dip, 0, sizeof(*dip));
			dip->type = type;
			log_write(bp); // mark it allocated on the disk
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}

	printf("ialloc: no inodes\n");
	return 0;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *idup(struct inode *ip)
{
	acquire(&itable.lock);
	ip->ref++;
	release(&itable.lock);
	return ip;
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void iupdate(struct inode *ip)
{
	struct buf	  *bp;
	struct dinode *dip;

	bp		   = bread(ip->dev, IBLOCK(ip->inum, sb));
	dip		   = (struct dinode *)bp->data + ip->inum % IPB;
	dip->type  = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->nlink = ip->nlink;
	dip->size  = ip->size;
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	log_write(bp);
	brelse(bp);
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void ilock(struct inode *ip)
{
	struct buf	  *bp;
	struct dinode *dip;

	if (ip == 0 || ip->ref < 1)
		panic("ilock");

	acquiresleep(&ip->lock);

	if (ip->is_valid == FALSE) {
		bp		  = bread(ip->dev, IBLOCK(ip->inum, sb));
		dip		  = (struct dinode *)bp->data + ip->inum % IPB;
		ip->type  = dip->type;
		ip->major = dip->major;
		ip->minor = dip->minor;
		ip->nlink = dip->nlink;
		ip->size  = dip->size;
		memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
		brelse(bp);
		ip->is_valid = TRUE;
		if (ip->type == 0)
			panic("ilock: no type");
	}
}

// Unlock the given inode.
void iunlock(struct inode *ip)
{
	if (ip == 0 || !is_holdingsleep(&ip->lock) || ip->ref < 1)
		panic("iunlock");

	releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode *ip)
{
	acquire(&itable.lock);

	if (ip->ref == 1 && ip->is_valid && ip->nlink == 0) {
		// inode has no links and no other references: truncate and free.

		// ip->ref == 1 means no other process can have ip locked,
		// so this acquiresleep() won't block (or deadlock).
		acquiresleep(&ip->lock);

		release(&itable.lock);

		itrunc(ip);
		ip->type = 0;
		iupdate(ip);
		ip->is_valid = 0;

		releasesleep(&ip->lock);

		acquire(&itable.lock);
	}

	ip->ref--;
	release(&itable.lock);
}

// Common idiom: unlock, then put.
void iunlockput(struct inode *ip)
{
	iunlock(ip);
	iput(ip);
}

// === inode 的读取、写入 ===
// Truncate inode (discard contents).
// Caller must hold ip->lock.
void itrunc(struct inode *ip)
{
	int			i, j;
	struct buf *bp;
	uint	   *a;

	for (i = 0; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	if (ip->addrs[NDIRECT]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT]);
		a  = (uint *)bp->data;
		for (j = 0; j < NINDIRECT; j++) {
			if (a[j]) {
				bfree(ip->dev, a[j]);
			}
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT]);
		ip->addrs[NDIRECT] = 0;
	}

	ip->size = 0;
	iupdate(ip);
}

// 拷贝inode状态
void stati(struct inode *ip, struct stat *st)
{
	st->dev	  = ip->dev;
	st->ino	  = ip->inum;
	st->type  = ip->type;
	st->nlink = ip->nlink;
	st->size  = ip->size;
}

// 读取inode中指向的数据
int readi(struct inode *ip, bool_t is_user_dst, uint64 dst, uint off, uint n)
{
	uint		tot, m;
	struct buf *bp;

	if (off > ip->size || off + n < off) {
		return 0;
	}
	if (off + n > ip->size) { // 大于文件最大值则只取最大值
		n = ip->size - off;
	}

	for (tot = 0; tot < n; tot += m, off += m, dst += m) {
		uint64 addr = bmap(ip, off / BSIZE); // 块号
		if (addr == 0) {
			break;
		}
		bp = bread(ip->dev, addr); // 读取实际数据内容
		m  = MIN(n - tot, BSIZE - off % BSIZE); // 解决未读取一个整块和偏移问题
		// m此时表示当前块中，具体要读取的字节数
		// 拷贝到用户地址或内核地址，啮合地址对等映射，用户地址需要查找到对应的物理地址
		if (either_copyout(is_user_dst, dst, (char *)(bp->data + (off % BSIZE)), m) == -1) {
			brelse(bp);
			tot = -1;
			break;
		}
		brelse(bp);
	}
	return tot;
}

int writei(struct inode *ip, bool_t is_user_src, uint64 src, uint off, uint n)
{
	uint		tot, m;
	struct buf *bp;

	if (off > ip->size || off + n < off) {
		return -1;
	}
	if (off + n > MAXFILE * BSIZE) {
		return -1;
	}

	for (tot = 0; tot < n; tot += m, off += m, src += m) {
		uint64 addr = bmap(ip, off / BSIZE); // 块号
		if (addr == 0) {
			break;
		}
		bp = bread(ip->dev, addr);
		m  = MIN(n - tot, BSIZE - off % BSIZE);
		if (either_copyin(is_user_src, (char *)(bp->data + (off % BSIZE)), src, m) == -1) {
			brelse(bp);
			break;
		}
		log_write(bp); // 写入实际数据
		brelse(bp);
	}

	if (off > ip->size) {
		ip->size = off;
	}

	iupdate(ip);
	return tot;
}

// directories 内容的获取、写入
int namecmp(const char *s, const char *t)
{
	return strncmp(s, t, DIRSIZ);
}

// inode是目录，查询特定名字的条目
// poff：若存在，则返回条目所在的inode数据区中的便宜
struct inode *dirlookup(struct inode *dp, char *name, uint *poff)
{
	uint		  off, inum;
	struct dirent de;

	if (dp->type != I_DIR) {
		panic("dirlookup: not a DIR");
	}

	for (off = 0; off < dp->size; off += sizeof(de)) {
		if ((readi(dp, 0, (uint64)&de, off, sizeof(de))) != sizeof(de)) {
			panic("dead dirlookup");
		}
		if (de.inum == 0) { // 空目录
			continue;
		}
		if (namecmp(name, de.name) == 0) {
			if (poff) {
				*poff = off; // 返回条目所在inode的偏移
			}
			inum = de.inum; // inode的编号，即为数组的索引
			return iget(dp->dev, inum);
		}
	}

	return 0;
}
/*
	dirlink(dp, name, inum)  // directory link
	├── 在目录 dp 中创建一个名为 name 的条目
	├── 指向 inode 编号 inum（文件或子目录）
	└── 返回 0 成功，-1 失败
*/
int dirlink(struct inode *dp, char *name, uint inum)
{
	int			  off;
	struct dirent de;
	struct inode *ip;

	// Check that name is not present.
	if ((ip = dirlookup(dp, name, 0)) != 0) {
		iput(ip);
		return -1;
	}

	// Look for an empty dirent.
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlink read");
		if (de.inum == 0)
			break;
	}

	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;
	if (writei(dp, FALSE, (uint64)&de, off, sizeof(de)) != sizeof(de))
		return -1;

	return 0;
}

// path 内容的获取、写入
// 获取第一层路径到name，返回之后的路径
/* 
    例如：
        a/aa/c name = a return aa/c
        ///a//bb name = a return bb
        a name = a return ""
*/

static char *skipelem(char *path, char *name)
{
	char *s;
	int	  len;

	while (*path == '/') { // 处理路径开头的/
		path++;
	}

	if (*path == 0) {
		return 0;
	}

	s = path; // a//b//c
	while (*path != '/' && *path != 0) {
		path++;
	}
	len = path - s; // //b//c  a//b//c 相当于求name的长度，求地址间的距离
	if (len >= DIRSIZ) {
		memmove(name, s, DIRSIZ);
	} else {
		memmove(name, s, len);
		name[len] = 0;
	}

	while (*path == '/') { // 去除剩余路径开头的斜线
		path++;
	}

	return path;
}

static struct inode *namex(char *path, bool_t nameiparent, char *name)
{
	struct inode *ip, *next;

	if (*path == '/') {
		ip = iget(ROOTDEV, ROOTINO);
	} else {
		ip = idup(myproc()->cwd);
	}

	while ((path = skipelem(path, name)) != 0) { // 表示剩余路径
		ilock(ip);
		if (ip->type != I_DIR) {
			iunlockput(ip);
			return 0;
		}
		if (nameiparent && *path == '\0') {
			iunlock(ip);
			return ip;
		}
		if ((next = dirlookup(ip, name, 0)) == 0) { // 查找下一层路径
			iunlock(ip);
			return 0;
		}
		iunlockput(ip);
		ip = next;
	}

	if (nameiparent) {
		iput(ip);
		return 0;
	}

	return ip;
}

// 查找路径最后一层的inode对象
struct inode *namei(char *path)
{
	char name[DIRSIZ];
	return namex(path, FALSE, name);
}

// 查找路径最后一层的父母路的inode对象
struct inode *nameiparent(char *path, char *name)
{
	return namex(path, TRUE, name);
}
