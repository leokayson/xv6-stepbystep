#ifndef __PROC_H__
#define __PROC_H__

#include "riscv.h"
#include "types.h"
#include "spinlock.h"
#include "param.h"

// 在内核上下文切换式，使用context
// 保存对象为属性callee的所有寄存器
struct context {
	uint64 ra;
	uint64 sp;

	// callee-saved
	uint64 s0;
	uint64 s1;
	uint64 s2;
	uint64 s3;
	uint64 s4;
	uint64 s5;
	uint64 s6;
	uint64 s7;
	uint64 s8;
	uint64 s9;
	uint64 s10;
	uint64 s11;
};

struct cpu {
	struct process *proc;
	struct context	context;
	int				nesting; // 嵌套深度 Depth of push_off() nesting
	int				intr_is_enabled; // 中断关闭前的中断状态
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
	/*   0 */ uint64 kernel_satp; // kernel page table
	/*   8 */ uint64 kernel_sp; // top of process's kernel stack
	/*  16 */ uint64 kernel_trap; // usertrap()
	/*  24 */ uint64 epc; // saved user program counter
	/*  32 */ uint64 kernel_hartid; // saved kernel tp
	/*  40 */ uint64 ra;
	/*  48 */ uint64 sp;
	/*  56 */ uint64 gp;
	/*  64 */ uint64 tp;
	/*  72 */ uint64 t0;
	/*  80 */ uint64 t1;
	/*  88 */ uint64 t2;
	/*  96 */ uint64 s0;
	/* 104 */ uint64 s1;
	/* 112 */ uint64 a0;
	/* 120 */ uint64 a1;
	/* 128 */ uint64 a2;
	/* 136 */ uint64 a3;
	/* 144 */ uint64 a4;
	/* 152 */ uint64 a5;
	/* 160 */ uint64 a6;
	/* 168 */ uint64 a7;
	/* 176 */ uint64 s2;
	/* 184 */ uint64 s3;
	/* 192 */ uint64 s4;
	/* 200 */ uint64 s5;
	/* 208 */ uint64 s6;
	/* 216 */ uint64 s7;
	/* 224 */ uint64 s8;
	/* 232 */ uint64 s9;
	/* 240 */ uint64 s10;
	/* 248 */ uint64 s11;
	/* 256 */ uint64 t3;
	/* 264 */ uint64 t4;
	/* 272 */ uint64 t5;
	/* 280 */ uint64 t6;
};

// Per-process state
struct process {
	struct spinlock lock;

	// p->lock must be held when using these:
	enum procstate state; // Process state
	void		  *chan; // If non-zero, sleeping on chan. 保存睡眠原因
	// int			   killed; // If non-zero, have been killed
	// int			   xstate; // Exit status to be returned to parent's wait
	int pid; // Process ID

	// wait_lock must be held when using this:
	struct process *parent; // Parent process。修改此对象必须持有wait_lock锁

	// these are private to the process, so p->lock need not be held.fd
	uint64			  kstack; // Virtual address of kernel stack
	uint64			  sz; // Size of process memory (bytes)
	pagetable_t		  pagetable; // User page table, 进程页表
	struct trapframe *trapframe; // data page for trampoline.S
	struct context	  context; // swtch() here to run process
	struct file		 *ofile[NOFILE]; // 文件对象
	struct inode	 *cwd; // Current directory
	char			  name[16]; // Process name (debugging)
};

int			cpuid();
struct cpu *mycpu();

void procinit();
void user1();
void user2();
void userinit1();
void userinit2();

void			proc_mapstacks(pagetable_t kpgtbl);
int				allocpid();
pagetable_t		proc_pagetable(struct process *p);
void			proc_freepagetable(pagetable_t pagetable, uint64 sz);
void			forkret();
struct process *myproc();
void			yield();
void			sched();
void			scheduler();
void			trace_next_pid(char *from);

void userinit();
// 将当前进程设置为睡眠状态
void sleep(void *chan, struct spinlock *lk);
// 唤醒
void wakeup(void *chan);

// 内核 -> 内核/用户 in proc
int either_copyout(bool_t is_user_dst, uint64 dst, char *src, uint64 len);
// 内核/用户 -> 内核 in proc
int either_copyin(bool_t is_user_src, char *dst, uint64 src, uint64 len);

int fork();

#endif // !__PROC_H__
