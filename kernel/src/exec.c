#include "exec.h"
#include "defs.h"
#include "elf.h"
#include "file.h"
#include "fs.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"
#include "log.h"
#include "vm.h"
#include "string.h"
#include "printf.h"

int		   exec(char *path, char **argv);
int		   flags2perm(int flags);
static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz);

int exec(char *path, char **argv)
{
	char		   *s, *last;
	uint			i, off;
	uint64			argc, sz = 0, sp, ustack[MAXARG], stackbase;
	struct elfhdr	elf;
	struct inode   *ip;
	struct proghdr	ph;
	pagetable_t		pagetable = 0, oldpagetable;
	struct process *p		  = myproc();

	begin_op();
	if ((ip = namei(path)) == 0) {
		end_op();
		return -1;
	}
	ilock(ip);

	// 检测elf头
	if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)) {
		goto bad;
	}
	if (elf.magic != ELF_MAGIC) {
		goto bad;
	}
	if ((pagetable = proc_pagetable(p)) == 0) {
		goto bad;
	}

	/*
    Program Headers:
    Type           Offset             VirtAddr           PhysAddr
                FileSiz            MemSiz              Flags  Align
    RISCV_ATTRIBUT 0x000000000000aad0 0x0000000000000000 0x0000000000000000
                0x000000000000005e 0x0000000000000000  R      0x1
    LOAD           0x0000000000001000 0x0000000080000000 0x0000000080000000
                0x0000000000009ad0 0x0000000000024738  RWE    0x1000
    GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                0x0000000000000000 0x0000000000000000  RW     0x10
*/
	// load program into memory
	for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
		// 读取program节
		if ((readi(ip, 0, (uint64)&ph, off, sizeof(ph))) != sizeof(ph)) {
			goto bad;
		}
		if (ph.type != ELF_PROG_LOAD) {
			continue;
		}
		// 1. 检查内存大小是否小于文件大小
		// 逻辑：在 ELF 规范中，内存占用 (memsz) 必须 >= 文件占用 (filesz)。
		// 原因：如果文件比内存大，说明文件里包含的数据无法完全装入分配的内存段中，
		// 这通常意味着文件已损坏或被恶意篡改。
		if (ph.memsz < ph.filesz) {
			goto bad;
		}
		// 2. 检查虚拟地址加法是否溢出
		// 逻辑：检查 (vaddr + memsz) 是否小于 (vaddr)。
		// 原因：这是经典的**整数溢出检测**。
		// 正常情况下，vaddr + memsz 应该大于 vaddr。
		// 如果攻击者构造了一个极大的 memsz，导致两者相加超过了 CPU 寄存器的上限（回绕），
		// 结果反而会变小。如果不检查这一点，系统可能会将数据写入到错误的内存地址（例如内核空间），导致严重的安全漏洞。
		if (ph.vaddr + ph.memsz < ph.vaddr) {
			goto bad;
		}
		if (ph.vaddr % PGSIZE != 0) {
			goto bad;
		}

		uint64 sz1;
		if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0) {
			goto bad;
		}
		sz = sz1;
		// 内部读取实际内容
		if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
			goto bad;
		}
	}
	iunlockput(ip);
	end_op();
	ip = 0;

	p			 = myproc();
	uint64 oldsz = p->sz;

	// 分配2个页作为分界标记
	// 1. 不可访问，作为stack guard
	// 2. 用户栈
	sz = PGROUNDUP(sz);
	uint64 sz1;
	if ((sz1 = uvmalloc(pagetable, sz, sz + 2 * PGSIZE, PTE_W)) == 0) {
		goto bad;
	}
	sz = sz1;
	uvmclear(pagetable, sz - 2 * PGSIZE);
	sp		  = sz;
	stackbase = sp - PGSIZE;

	// push argument strings, prepare rest of stack in ustack
	for (argc = 0; argv[argc]; argc++) {
		if (argc >= MAXARG) {
			goto bad;
		}
		sp -= strlen(argv[argc]) + 1;
		sp -= sp % 16; // riscv sp: 16B aligned
		if (sp < stackbase) {
			goto bad;
		}
		if (copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
			goto bad;
		}
		ustack[argc] = sp;
	}
	ustack[argc] = 0;

	// push the array of argv[] pointers.
	sp -= (argc + 1) * sizeof(uint64);
	sp -= sp % 16;
	if (sp < stackbase) {
		goto bad;
	}
	if (copyout(pagetable, sp, (char *)ustack, (argc + 1) * sizeof(uint64)) < 0) {
		goto bad;
	}

	// args to user main(argc, argv)
	// argc is returned via the system call return
	// value, which goes in a0
	p->trapframe->a1 = sp;

	// save program name for debugging
	for (last = s = path; *s; s++) {
		if (*s == '/') {
			last = s + 1;
		}
	}
	safestrcpy(p->name, last, sizeof(p->name));

	// commit to the user image
	oldpagetable	  = p->pagetable;
	p->pagetable	  = pagetable;
	p->sz			  = sz;
	p->trapframe->epc = elf.entry; // inital pc = main
	p->trapframe->sp  = sp; // initial stack pointer
	proc_freepagetable(oldpagetable, oldsz);

	return argc; // this ends up in a0, the first arg to main(argc, argv)

bad:
	if (pagetable) {
		proc_freepagetable(pagetable, sz);
	}
	if (ip) {
		iunlockput(ip);
		end_op();
	}
	return -1;
}

int flags2perm(int flags)
{
	int perm = 0;
	if (flags & 0x1) {
		perm = PTE_X;
	}
	if (flags & 0x2) {
		perm |= PTE_W;
	}
	return perm;
}

static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
	uint   i, n;
	uint64 pa;

	for (i = 0; i < sz; i += PGSIZE) {
		pa = walkaddr(pagetable, va + i);
		if (pa == 0) {
			panic("loadseg: address should exist");
		}
		n = MIN(sz - i, PGSIZE);
		if (readi(ip, 0, (uint64)pa, offset + i, n) != n) {
			return -1;
		}
	}

	return 0;
}
