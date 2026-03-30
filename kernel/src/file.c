#include "file.h"
#include "fs.h"
#include "log.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "defs.h"
#include "printf.h"
#include "types.h"
#include "vm.h"

struct devsw devsw[NDEV];
struct {
	struct spinlock lock;
	struct file		file[NFILE];
} ftable;

/*
    void fileinit();
    struct file *filealloc();
    struct file *fileup(struct file *f);
    void fileclose(struct file *f);
    int filestat(struct file *f, uint64 addr);
    int fileread(struct file *f, uint64 addr, int n);
    int filewrite(struct file *f, uint64 addr, int n);
*/

void fileinit()
{
	initlock(&ftable.lock, "ftable");
}

// 分配一个文件对象（在ftable中找到一个空位）
struct file *filealloc()
{
	struct file *f;

	acquire(&ftable.lock);
	for (f = ftable.file; f < ftable.file + NFILE; ++f) {
		if (f->ref == 0) {
			f->ref = 1;
			release(&ftable.lock);
			return f;
		}
	}
	release(&ftable.lock);

	return NULL;
}

// 增加对文件对象的引用计数（硬链接）
struct file *fileup(struct file *f)
{
	acquire(&ftable.lock);
	if (f->ref < 1) {
		panic("fileup");
	}
	f->ref++;
	release(&ftable.lock);

	return f;
}

// 关闭文件对象，若引用为0则关闭文件
void fileclose(struct file *f)
{
	struct file ff;

	acquire(&ftable.lock);
	if (f->ref < 1) {
		panic("fileclose");
	}
	if (--f->ref > 0) {
		release(&ftable.lock);
		return;
	}
	ff		= *f;
	f->type = F_NONE;

	if (f->type == F_PIPE) {
		// TODO 关闭管道对象
	} else if (f->type == F_INODE || f->type == F_DEVICE) {
		begin_op();
		iput(ff.ip);
		end_op();
	}

	release(&ftable.lock);
}

// 获取文件对象的元数据。addr (va in user)
int filestat(struct file *f, uint64 addr)
{
	struct process *p = myproc();
	struct stat		st;

	if (f->type == F_INODE || f->type == F_DEVICE) {
		ilock(f->ip);
		stati(f->ip, &st);
		iunlock(f->ip);
		if ((copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)) {
			return -1;
		}
		return 0;
	}

	return -1;
}

// file content -> addr (va in user)
int fileread(struct file *f, uint64 addr, int n)
{
	int r = 0;

	if (f->readable == FALSE) {
		return -1;
	}

	switch (f->type) {
		case F_PIPE: {
			// TODO 管道读取
			break;
		}
		case F_DEVICE: {
			// TODO 设备读取
			break;
		}
		case F_INODE: {
			ilock(f->ip);
			if ((r = readi(f->ip, TRUE, addr, f->off, n)) > 0) {
				f->off += r;
			}
			iunlock(f->ip);
		}
		default: {
			panic("fileread UNDEFINED device");
		}
	}

	return r;
}

// addr (va in user) -> file content
int filewrite(struct file *f, uint64 addr, int n)
{
	int r, ret = 0;

	if (f->writable == FALSE) {
		return -1;
	}

	switch (f->type) {
		case F_PIPE: {
			// TODO 管道写入
			break;
		}
		case F_DEVICE: {
			// TODO 设备写入
			break;
		}
		case F_INODE: {
			/* inode, indirect block, allocation blocks (可能处于2个页中) */
			int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
			int i	= 0;

			while (i < n) {
				int nl = MIN(n - i, max);

				begin_op();
				ilock(f->ip);
				if ((r = writei(f->ip, TRUE, addr, f->off, n)) > 0) {
					f->off += r;
				}
				iunlock(f->ip);

				if (r != nl) {
					// error from writei
					break;
				}
			}
			ret = (i == n ? n : -1);
		}
		default: {
			panic("filewrite UNDEFINED device");
		}
	}

	return ret;
}
