#ifndef __FS_H__
#define __FS_H__

#include "types.h"
#include "file.h"

#define ROOTINO 1 // root i-number

#define BSIZE	1024 // block size
#define FSMAGIC 0x10203040

// Disk layout:
// [ boot block | super block | log | inode blocks | free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
	uint magic; // Must be FSMAGIC
	uint size; // Size of file system image (blocks)
	uint nblocks; // Number of data blocks
	uint ninodes; // Number of inodes.
	uint nlog; // Number of log blocks
	uint logstart; // Block number of first log block
	uint inodestart; // Block number of first inode block
	uint bmapstart; // Block number of first free map block
};

// On-disk inode structure
//
struct dinode {
	short type; // File type
	short major; // Major device number (T_DEVICE only)
	short minor; // Minor device number (T_DEVICE only)
	short nlink; // Number of links to inode in file system
	uint  size; // Size of file (bytes)
	uint  addrs[NDIRECT + 1]; // Data block addresses
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
	ushort inum;
	char   name[DIRSIZ];
};

// fs.c
void fsinit(int dev);
// blocks 的分配和释放

// inodes 的分配、封信、释放
void		  iinit();
struct inode *ialloc(uint dev, inodetype type);
struct inode *idup(struct inode *ip);
void		  iupdate(struct inode *ip);
void		  ilock(struct inode *ip);
void		  iunlock(struct inode *ip);
void		  iput(struct inode *ip);
void		  iunlockput(struct inode *ip);

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
int	 readi(struct inode *ip, bool is_user_dst, uint64 dst, uint off, uint n);
int	 writei(struct inode *ip, bool is_user_src, uint64 src, uint off, uint n);

// directories 内容的获取、写入
int			  namecmp(const char *s, const char *t);
struct inode *dirlookup(struct inode *dp, char *name, uint *poff);
int			  dirlink(struct inode *dp, char *name, uint inum);

// paths 内容的获取、写入
struct inode		*namei(char *path);
struct inode		*nameiparent(char *path, char *name);

#endif // !__FS_H__
