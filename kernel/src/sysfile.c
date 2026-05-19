#include "defs.h"
#include "exec.h"
#include "fcntl.h"
#include "fs.h"
#include "param.h"
#include "pipe.h"
#include "proc.h"
#include "riscv.h"
#include "syscall.h"
#include "types.h"
#include "file.h"
#include "printf.h"
#include "log.h"
#include "kalloc.h"
#include "string.h"
#include "vm.h"

__attribute__((unused)) static int isdirempty(struct inode *dp);
static struct inode				  *create(char *path, inodetype type, short major, short minor);
static int						   argfd(int n, int *pfd, struct file **pf);
static int						   fdalloc(struct file *f);

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

// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp)
{
	int			  off;
	struct dirent de;

	for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
		if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
			panic("isdirempty: readi");
		if (de.inum != 0)
			return 0;
	}
	return 1;
}

// 将给定的文件存放在当前进程的文件数组中
static int fdalloc(struct file *f)
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
	struct file *f;
	int			 fd;

	if (argfd(0, 0, &f) < 0) {
		return -1;
	}
	if ((fd = fdalloc(f)) < 0) {
		return -1;
	}
	fileup(f);

	return 0;
}

uint64 sys_read()
{
	struct file *f;
	int			 n;
	uint64		 p;

	argaddr(1, &p);
	argint(2, &n);

	if (argfd(0, 0, &f) < 0) {
		return -1;
	}

	return fileread(f, p, n);
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
	struct file *f;
	uint64		 st; // 用户态地址

	argaddr(1, &st);
	if (argfd(0, 0, &f) < 0) {
		return -1;
	}

	return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64 sys_link(void)
{
	char		  name[DIRSIZ], new[MAXPATH], old[MAXPATH];
	struct inode *dp, *ip;

	if (argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
		return -1;

	begin_op();
	if ((ip = namei(old)) == 0) {
		end_op();
		return -1;
	}

	ilock(ip);
	if (ip->type == I_DIR) {
		iunlockput(ip);
		end_op();
		return -1;
	}

	ip->nlink++;
	iupdate(ip);
	iunlock(ip);

	if ((dp = nameiparent(new, name)) == 0)
		goto bad;
	ilock(dp);
	if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
		iunlockput(dp);
		goto bad;
	}
	iunlockput(dp);
	iput(ip);

	end_op();

	return 0;

bad:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	end_op();
	return -1;
}

uint64 sys_unlink()
{
	struct inode *ip, *dp;
	struct dirent de;
	char		  name[DIRSIZ], path[MAXPATH];
	uint		  off;

	if (argstr(0, path, MAXPATH) < 0)
		return -1;

	begin_op();
	if ((dp = nameiparent(path, name)) == 0) {
		end_op();
		return -1;
	}

	ilock(dp);

	// Cannot unlink "." or "..".
	if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto bad;

	if ((ip = dirlookup(dp, name, &off)) == 0)
		goto bad;
	ilock(ip);

	if (ip->nlink < 1)
		panic("unlink: nlink < 1");
	if (ip->type == I_DIR && !isdirempty(ip)) {
		iunlockput(ip);
		goto bad;
	}

	memset(&de, 0, sizeof(de));
	if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
		panic("unlink: writei");
	if (ip->type == I_DIR) {
		dp->nlink--;
		iupdate(dp);
	}
	iunlockput(dp);

	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);

	end_op();

	return 0;

bad:
	iunlockput(dp);
	end_op();
	return -1;
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
	if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) { // 创建失败则返回
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
	f->ip		   = ip; // 关联文件对象和inode
	f->is_readable = !(omode & O_WRONLY);
	f->is_writable = (omode & O_WRONLY) || (omode & O_RDWR);

	if ((omode & O_TRUNC) && ip->type == I_FILE) {
		itrunc(ip);
	}

	iunlock(ip);
	end_op();

	return fd; // 返回文件句柄，即数组号
}

uint64 sys_mkdir()
{
	char		  path[MAXPATH];
	struct inode *ip;

	begin_op();
	if (argstr(0, path, MAXPATH) < 0 || (ip = create(path, I_DIR, 0, 0)) == 0) {
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

uint64 sys_mknod()
{
	struct inode *ip;
	char		  path[MAXPATH];
	int			  major, minor;

	begin_op();
	argint(1, &major);
	argint(2, &minor);
	if ((argstr(0, path, MAXPATH)) < 0 || (ip = create(path, I_DEVICE, major, minor)) == 0) {
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

uint64 sys_chdir()
{
	char			path[MAXPATH];
	struct inode   *ip;
	struct process *p = myproc();

	begin_op();
	if (argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0) {
		goto bad;
	}
	ilock(ip);
	if (ip->type != I_DIR) {
		iunlockput(ip);
		goto bad;
	}
	iunlock(ip);
	iput(p->cwd);
	end_op();
	p->cwd = ip;
	return 0;

bad:
	end_op();
	return -1;
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
	uint64			fdarray; // user pointer to array of two integers
	struct file	   *rf, *wf;
	int				fd0, fd1;
	struct process *p = myproc();

	argaddr(0, &fdarray);
	if (pipealloc(&rf, &wf) < 0)
		return -1;
	fd0 = -1;
	if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) {
		if (fd0 >= 0)
			p->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -1;
	}
	if (copyout(p->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) < 0 ||
		copyout(p->pagetable, fdarray + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0) {
		p->ofile[fd0] = 0;
		p->ofile[fd1] = 0;
		fileclose(rf);
		fileclose(wf);
		return -1;
	}
	return 0;
}
