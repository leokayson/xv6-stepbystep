#include "defs.h"
#include "riscv.h"
#include "param.h"
#include "types.h"
#include "proc.h"
#include "memlayout.h"
#include "spinlock.h"
#include "printf.h"
#include "uart.h"
#include "kalloc.h"
#include "vm.h"
#include "trap.h"
#include "string.h"
#include "fs.h"

extern char		trampoline[]; // defined in trampoline.S
struct cpu		cpus[NCPU];
struct process	proc[NPROC];
uchar			stack1[PGSIZE];
uchar			stack2[PGSIZE];
struct spinlock pid_lock;
int nextpid = 1;
struct spinlock pid_lock; // 全局进程号锁

static int next_runnable_pid(struct process *current)
{
	int start = 0;

	if (current)
		start = (int)(current - proc) + 1;

	for (int off = 0; off < NPROC; ++off) {
		struct process *candidate = &proc[(start + off) % NPROC];
		if (candidate->state == RUNNABLE) {
			return candidate->pid;
		}
	}

	return -1;
}

void trace_next_pid(char *from)
{
	struct process *current = myproc();
	int				nextpid = next_runnable_pid(current);

	if (current)
		printf("[%s] current pid = %d, next pid = %d\n", from, current->pid, nextpid);
	else
		printf("[%s] current pid = -1, next pid = %d\n", from, nextpid);
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
	int id = r_tp();
	return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *mycpu(void)
{
	int			id = cpuid();
	struct cpu *c  = &cpus[id];
	return c;
}

/*
	void procinit();
	void user1();
	void user2();
	void userinit1();
	void userinit2();
	
	void proc_mapstack(pagetable_t kpgtbl);
	int allocpid();
	void proc_pagetable(struct process *p, uint64 sz);
	void proc_freepagetable(pagetable_t *p);
	void forkret();
	struct process *myproc();
	void yield();
	void sched();
	void scheduler();
	void userinit();
	void userinit1();
*/

// 初始化全局进程表
void procinit()
{
	struct process *p;
	initlock(&pid_lock, "nextpid");

	for (p = proc; p < &proc[NPROC]; ++p) {
		initlock(&p->lock, "proc");
		p->state  = UNUSED;
		p->kstack = KSTACK((int)(p - proc));
	}
}

// 第一个进程的执行体
void user1()
{
	while (1) {
		uartsleep(2);

		printf("a ");
	}
}

// 第二个进程的执行体
void user2()
{
	while (1) {
		uartsleep(2);

		printf("b ");
	}
}

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
	struct process *p;

	for (p = proc; p < &proc[NPROC]; p++) {
		char *pa = kalloc();
		if (pa == 0)
			panic("kalloc");
		uint64 va = KSTACK((int)(p - proc));
		kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_RW);
	}
}

int allocpid()
{
	int pid;

	acquire(&pid_lock);
	pid		= nextpid;
	nextpid = nextpid + 1;
	release(&pid_lock);

	return pid;
};

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t proc_pagetable(struct process *p)
{
	pagetable_t pagetable;

	// An empty page table.
	pagetable = uvmcreate();
	if (pagetable == 0)
		return 0;

	// map the trampoline code (for system call return)
	// at the highest user virtual address.
	// only the supervisor uses it, on the way
	// to/from user space, so not PTE_U.
	if (mappages(pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_RX) < 0) {
		uvmfree(pagetable, 0);
		return 0;
	}

	// map the trapframe page just below the trampoline page, for
	// trampoline.S.
	if (mappages(pagetable, TRAPFRAME, (uint64)(p->trapframe), PGSIZE, PTE_RW) < 0) {
		uvmunmap(pagetable, TRAMPOLINE, 1, 0);
		uvmfree(pagetable, 0);
		return 0;
	}

	return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
	uvmunmap(pagetable, TRAMPOLINE, 1, 0);
	uvmunmap(pagetable, TRAPFRAME, 1, 0);
	uvmfree(pagetable, sz);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
	static int first = 1;

	// Still holding p->lock from scheduler.
	release(&myproc()->lock);

	if (first) {
		// File system initialization must be run in the context of a
		// regular process (e.g., because it calls sleep), and thus cannot
		// be run from main().
		
		first = 0;
		fsinit(ROOTDEV);
		// ensure other cores see first=0.
		__sync_synchronize();
	}

	usertrapret();
}

// Return the current struct proc *, or zero if none.
struct process *myproc(void)
{
	critical_enter();
	struct cpu	   *c = mycpu();
	struct process *p = c->proc;
	critical_exit();

	return p;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
	struct process *p = myproc();
	acquire(&p->lock);
	p->state = RUNNABLE;
	sched();
	release(&p->lock);
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
	int				intr_enabled;
	struct process *p = myproc();

	if (!holding(&p->lock))
		panic("sched p->lock");
	//   if(mycpu()->noff != 1)
	//     panic("sched locks");
	if (p->state == RUNNING)
		panic("sched running");
	if (intr_get())
		panic("sched interruptible");

	intr_enabled = mycpu()->intr_is_enabled;
	swtch(&p->context, &mycpu()->context);
	mycpu()->intr_is_enabled = intr_enabled;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
	struct process *p;
	struct cpu	   *c = mycpu();

	c->proc = 0;
	for (;;) {
		// The most recent process to run may have had interrupts
		// turned off; enable them to avoid a deadlock if all
		// processes are waiting.
		intr_on();

		for (p = proc; p < &proc[NPROC]; p++) {
			acquire(&p->lock);
			if (p->state == RUNNABLE) {
				// Switch to chosen process.  It is the process's job
				// to release its lock and then reacquire it
				// before jumping back to us.
				printf("[scheduler] next pid = %d\n\n", p->pid);
				p->state = RUNNING;
				c->proc	 = p;
				swtch(&c->context, &p->context);

				// Process is done running for now.
				// It should have changed its p->state before coming back.
				c->proc = 0;
			}
			release(&p->lock);
		}
	}
}

#if 0
// 初始化第一个进程
void user1init()
{
	proc[0].context.sp = (uint64) & (stack1[PGSIZE - 1]);
	proc[0].context.pc = (uint64)(&user1);
}

// 初始化第二个进程
void user2init()
{
	proc[1].context.sp = (uint64) & (stack2[PGSIZE - 1]);
	proc[1].context.pc = (uint64)(&user2);
}
#endif

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void freeproc(struct process *p)
{
	if (p->trapframe)
		kfree((void *)p->trapframe);
	p->trapframe = 0;
	if (p->pagetable)
		proc_freepagetable(p->pagetable, p->sz);
	p->pagetable = 0;
	//   p->sz = 0;
	p->pid = 0;
	//   p->parent = 0;
	p->name[0] = 0;
	//   p->chan = 0;
	//   p->killed = 0;
	//   p->xstate = 0;
	p->state = UNUSED;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct process *allocproc(void)
{
	/*
		1. 查找进程号，获得一个未使用的位置
		2. 获取进程号
		3. 分配trapframe
		4. 分配用户页表
		5. 设置内核态ra sp
	*/
	struct process *p;

	for (p = proc; p < &proc[NPROC]; p++) {
		acquire(&p->lock);
		if (p->state == UNUSED) {
			goto found;
		} else {
			release(&p->lock);
		}
	}
	return 0;

found:
	p->pid	 = allocpid();
	p->state = USED;

	// Allocate a trapframe page.
	if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
		freeproc(p);
		release(&p->lock);
		return 0;
	}

	// An empty user page table.
	p->pagetable = proc_pagetable(p);
	if (p->pagetable == 0) {
		freeproc(p);
		release(&p->lock);
		return 0;
	}

	// Set up new context to start executing at forkret,
	// which returns to user space.
	memset(&p->context, 0, sizeof(p->context));
	p->context.ra = (uint64)forkret;
	p->context.sp = p->kstack + PGSIZE;

	return p;
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
// uchar initcode[] = { 0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02, 0x97, 0x05, 0x00, 0x00, 0x93,
// 					 0x85, 0x35, 0x02, 0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00, 0x93, 0x08,
// 					 0x20, 0x00, 0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e,
// 					 0x69, 0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*
uchar initcode[]  = { 0x1b, 0x05, 0x10, 0x00, 0x9b, 0x05, 0x20, 0x00, 0x1b, 0x06, 0x00, 0x00, 0x33, 0x06,
					  0xb5, 0x00, 0x6f, 0xf0, 0x1f, 0xff, 0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x00 };
*/
#if 0
#if 1
// Positive test: 测试通过系统调用号为22
uchar initcode[] = { 0x13, 0x05, 0x20, 0x02, 0x93, 0x05, 0x30, 0x03, 0x93, 0x08, 0x60, 0x01, 0x73, 0x00, 0x00,
					 0x00, 0x13, 0x05, 0x40, 0x04, 0x93, 0x05, 0x50, 0x05, 0x6f, 0xf0, 0x9f, 0xfe, 0x2f, 0x69,
					 0x6e, 0x69, 0x74, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
#else // 1
// Negative test: 系统调用号为21
uchar initcode[]  = { 0x13, 0x05, 0x20, 0x02, 0x93, 0x05, 0x30, 0x03, 0x93, 0x08, 0x50, 0x01, 0x73, 0x00, 0x00,
					  0x00, 0x13, 0x05, 0x40, 0x04, 0x93, 0x05, 0x50, 0x05, 0x6f, 0xf0, 0x9f, 0xfe, 0x2f, 0x69,
					  0x6e, 0x69, 0x74, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
					  #endif // 1
#endif

uchar initcode[]  = { 
	0x17, 0x08, 0x00, 0x00, 0x13, 0x08, 0xc8, 0x04, 0x13, 0x05, 0x08, 0x00, 0x93, 0x05, 0x20, 0x20,
	0x93, 0x08, 0xf0, 0x00, 0x73, 0x00, 0x00, 0x00, 0x93, 0x08, 0x05, 0x00, 0x17, 0x08, 0x00, 0x00,
	0x13, 0x08, 0x98, 0x03, 0x93, 0x05, 0x08, 0x00, 0x13, 0x06, 0x60, 0x00, 0x93, 0x08, 0x00, 0x01,
	0x73, 0x00, 0x00, 0x00, 0x13, 0x85, 0x07, 0x00, 0x93, 0x08, 0x50, 0x01, 0x73, 0x00, 0x00, 0x00, 
	0x13, 0x05, 0x40, 0x04, 0x93, 0x05, 0x50, 0x05, 0x6f, 0xf0, 0x9f, 0xff, 0x2f, 0x61, 0x2e, 0x74, 
	0x78,  // a.txt
	// 0x13, 0x05, 0x40, 0x04, 0x93, 0x05, 0x50, 0x05, 0x6f, 0xf0, 0x9f, 0xff, 0x2f, 0x62, 0x2e, 0x74, 
	// 0x78,  // b.txt
	0x74, 0x00, 0x00, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x00, 0x00, 0x00
};
					  
uchar initcode1[] = { 0x1b, 0x05, 0x20, 0x00, 0x9b, 0x05, 0x30, 0x00, 0x1b, 0x06, 0x00, 0x00, 0x33, 0x06,
					  0xb5, 0x00, 0x6f, 0xf0, 0x1f, 0xff, 0x2f, 0x69, 0x6e, 0x69, 0x74, 0x31, 0x00, 0x00 };

// Set up first user process.
void userinit(void)
{
	struct process *p;

	p = allocproc();
	// initproc = p;

	// allocate one user page and copy initcode's instructions
	// and data into it.
	uvmfirst(p->pagetable, initcode, sizeof(initcode));
	p->sz = PGSIZE;

	// prepare for the very first "return" from kernel to user.
	p->trapframe->epc = 0; // user program counter
	p->trapframe->sp  = PGSIZE; // user stack pointer

	safestrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = namei("/");
	p->state = RUNNABLE;

	release(&p->lock);
}

// Set up second user process.
void userinit1(void)
{
	struct process *p;
	p = allocproc();

	// allocate one user page and copy initcode's instructions
	// and data into it.
	uvmfirst(p->pagetable, initcode1, sizeof(initcode1));
	p->sz = PGSIZE;

	// prepare for the very first "return" from kernel to user.
	p->trapframe->epc = 0; // user program counter
	p->trapframe->sp  = PGSIZE; // user stack pointer

	safestrcpy(p->name, "initcode1", sizeof(p->name));
	p->state = RUNNABLE;

	release(&p->lock);
}

// 睡眠函数
void uartsleep(int sec)
{
	// 1000000 * 10 * 2 同样是2s，为什么这个就少？
	// 可见这里通过自加的方式来计时十分不准确
	// qemu使用这种方式是，因为自增是QEMU实现的，所以相对比较准确
	int interval = 1000000 * 10 * sec;
	int count	 = 0;
	while (count++ < interval) {
	}
}

// 将当前进程设置为睡眠状态
void sleep(void* chan, struct spinlock *lk) {
	struct process *p = myproc();
	
	acquire(&p->lock);
	release(lk);
	p->chan = chan;
	p->state = SLEEPING;
	
	sched();
	
	p->chan = 0; // 之前已经切换到其他进程中了，当前进程再次被唤醒才会执行此代码
	
	release(&p->lock);
	acquire(lk);
}

// 唤醒
void wakeup(void* chan) {
	struct process *p;
	
	for (p = proc; p < &proc[NPROC]; ++p) {
		if (p != myproc()) { // 非当前进程才有可能处于睡眠状态
			acquire(&p->lock);
			if(p->state == SLEEPING && p->chan == chan) {
				p->state = RUNNABLE;
			}
			release(&p->lock);
		}
	}
}

// 内核 -> 内核/用户 in proc
int either_copyout(bool_t is_user_dst, uint64 dst, char *src, uint64 len) {
	struct process *p = myproc();
	if (is_user_dst) {
		return copyout(p->pagetable, dst, src, len);
	} else {
		memmove((char *)dst, src, len);
		return 0;
	}
}

// 内核/用户 -> 内核 in proc
int either_copyin(bool_t is_user_src, char *dst, uint64 src, uint64 len) {
	struct process *p = myproc();
	if (is_user_src) {
		return copyin(p->pagetable, dst, src, len);
	} else {
		memmove(dst, (char *)src, len);
		return 0;
	}
}
