#include "defs.h"
#include "exec.h"
#include "fcntl.h"
#include "fs.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "syscall.h"
#include "types.h"
#include "file.h"
#include "printf.h"
#include "log.h"
#include "kalloc.h"

__attribute__((unused)) static int isdirempty(struct inode *dp);
static struct inode				  *create(char *path, inodetype type, short major, short minor);
static int						   argfd(int n, int *pfd, struct file **pf);
static int						   falloc(struct file *f);

uint64 sys_dup();
uint64 sys_read();
uint64 sys_write();
uint64 sys_close();
uint64 sys_fstat();
uint64 sys_link();
uint64 sys_unlink();
uint64 sys_mkdir();
uint64 sys_mknod();
uint64 sys_chdir();
uint64 sys_exec();
uint64 sys_pipe();
uint64 sys_open();

// 获取第n个系统调用参数，并将其视为文件描述符
static int argfd(int n, int *pfd, struct file **pf)
{
	int			 fd;
	struct file *f;

	argint(n, &fd);

	if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0) {
		return -1;
	}

	if (pfd) {
		*pfd = fd; // 返回文件对象在数组中的索引
	}
	if (pf) {
		*pf = f; // 返回文件对象；
	}

	return 0;
}

// 将给定的文件存放在当前进程的文件数组中
static int falloc(struct file *f)
{
	int				fd;
	struct process *p = myproc();

	for (fd = 0; fd < NOFILE; ++fd) {
		if (p->ofile[fd] == 0) {
			p->ofile[fd] = f; // 可见文件描述符就是1个数字，即数组的索引
			return fd;
		}
	}

	return -1;
}

uint64 sys_dup()
{
	// TODO
	return 0;
}

uint64 sys_read()
{
	// TODO
	return 0;
}

uint64 sys_write()
{
	struct file *f;
	int			 n;
	uint64		 p;

	argaddr(1, &p); // 要写入的字符串指针
	argint(2, &n); // 个数
	if (argfd(0, 0, &f) < 0) // 文件对象
		return -1;

	printf("sys_write\n");

	return filewrite(f, p, n);
}

uint64 sys_close()
{
	int			 fd;
	struct file *f;

	if (argfd(0, &fd, &f) < 0) // 文件对象
		return -1;

	printf("sys_close\n");
	myproc()->ofile[fd] = 0;
	fileclose(f);

	return 0;
}

uint64 sys_fstat()
{
	// TODO
	return 0;
}

uint64 sys_link()
{
	// TODO
	return 0;
}

uint64 sys_unlink()
{
	// TODO
	return 0;
}

__attribute__((unused)) static int isdirempty(struct inode *dp)
{
	// TODO
	return 0;
}

static struct inode *create(char *path, inodetype type, short major, short minor)
{
	struct inode *ip, *dp;
	char		  name[DIRSIZ];

	// 获取最后一层父目录的inode对象
	if ((dp = nameiparent(path, name)) == NULL) // 父目录不存在则直接返回
		return 0;

	ilock(dp);

	// 父目录存在此文件
	if ((ip = dirlookup(dp, name, 0)) != NULL) {
		iunlockput(dp);
		ilock(ip);
		if (type == I_FILE && (ip->type == I_FILE || ip->type == I_DEVICE))
			return ip;
		iunlockput(ip);
		return 0;
	}

	// 父目录不存在此文件，则创建
	if ((ip = ialloc(dp->dev, type)) == NULL) { // 创建失败
		iunlockput(dp);
		return 0;
	}

	// 创建inode成功
	ilock(ip);
	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;
	iupdate(ip);

	// 若为目录，则向目录创建.和..条目
	if (type == I_DIR) {
		if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
			goto fail;
	}

	// 将创建的inode添加到父目录中
	if (dirlink(dp, name, ip->inum) < 0)
		goto fail;

	if (type == I_DIR) {
		// 若创建为目录，则增加父目录
		dp->nlink++;
		iupdate(dp);
	}

	iunlockput(dp);

	return ip;

fail:
	// something went wrong. de-allocate ip.
	ip->nlink = 0;
	iupdate(ip);
	iunlockput(ip);
	iunlockput(dp);
	return 0;
}

// 打开文件对象
// 参数列表：路径，模式
uint64 sys_open(void)
{
	char		  path[MAXPATH];
	int			  fd, omode;
	struct file	 *f;
	struct inode *ip;
	int			  n;

	argint(1, &omode);
	if ((n = argstr(0, path, MAXPATH)) < 0)
		return -1;

	begin_op();

	if (omode & O_CREATE) { // 创建文件
		ip = create(path, I_FILE, 0, 0);
		if (ip == 0) {
			end_op();
			return -1;
		}
	} else { // 打开文件
		if ((ip = namei(path)) == 0) {
			end_op();
			return -1;
		}
		ilock(ip);
		if (ip->type == I_DIR && omode != O_RDONLY) {
			iunlockput(ip);
			end_op();
			return -1;
		}
	}

	// 获得inode成功
	if (ip->type == I_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
		iunlockput(ip);
		end_op();
		return -1;
	}

	// 创建文件对象
	if ((f = filealloc()) == 0 || (fd = falloc(f)) < 0) { // 创建失败则返回
		if (f)
			fileclose(f);
		iunlockput(ip);
		end_op();
		return -1;
	}

	if (ip->type == I_DEVICE) {
		f->type	 = F_DEVICE;
		f->major = ip->major;
	} else {
		f->type = F_INODE;
		f->off	= 0; // 文件默认偏移为0
	}
	f->ip		= ip; // 关联文件对象和inode
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

	if ((omode & O_TRUNC) && ip->type == I_FILE) {
		itrunc(ip);
	}

	iunlock(ip);
	end_op();

	return fd; // 返回文件句柄，即数组号
}

uint64 sys_mkdir()
{
	// TODO
	return 0;
}

uint64 sys_mknod()
{
	// TODO
	return 0;
}

uint64 sys_chdir()
{
	// TODO
	return 0;
}

uint64 sys_exec()
{
	char   path[MAXPATH], *argv[MAXARG];
	int	   i;
	uint64 uargv, uarg;

	// 参数列表 path, argv
	argaddr(1, &uargv);
	if (argstr(0, path, MAXPATH) < 0) {
		return -1;
	}

	for (i = 0;; ++i) {
		if (i >= NELEM(argv)) {
			goto bad;
		}
		if (fetchaddr(uargv + sizeof(uint64) * 1, (uint64 *)&uarg) < 0) {
			goto bad;
		}
		if (uarg == 0) {
			argv[i] = 0;
			break;
		}
		if ((argv[i] = kalloc()) == 0) {
			goto bad;
		}
		if (fetchstr(uarg, argv[i], PGSIZE) < 0) {
			goto bad;
		}
	}

	int ret = exec(path, argv);

	for (i = 0; i < NELEM(argv) && argv[i] != 0; ++i) {
		kfree(argv[i]);
	}

	return ret;
bad:
	for (i = 0; i < NELEM(argv) && argv[i] != 0; ++i) {
		kfree(argv[i]);
	}

	return -1;
}

uint64 sys_pipe()
{
	// TODO
	return 0;
}
