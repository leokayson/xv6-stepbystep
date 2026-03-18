#include "defs.h"
#include "riscv.h"
#include "param.h"
#include "types.h"
#include "proc.h"

struct cpu cpus[NCPU];

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

struct process proc[NPROC];
uchar stack1[PGSIZE];
uchar stack2[PGSIZE];
int nextpid = 1;
/*
	void procinit();
	void user1();
	void user2();
	void user1init();
	void user2init();
*/

// 初始化全局进程表
void procinit() {
	struct process *p;
	for (p = proc; p < &proc[NPROC]; ++p) {
		p->pid = nextpid;
		nextpid++;
	}
}

// 第一个进程的执行体
void user1() {
	while (1) {
		uartsleep(2);

		printf("a ");
	}
}

// 第二个进程的执行体
void user2() {
	while (1) {
		uartsleep(2);

		printf("b ");
	}
}

// 初始化第一个进程
void user1init() {
	proc[0].context.sp = (uint64)&(stack1[PGSIZE - 1]);
	proc[0].context.pc = (uint64)(&user1);
}

// 初始化第二个进程
void user2init() {
	proc[1].context.sp = (uint64)&(stack2[PGSIZE - 1]);
	proc[1].context.pc = (uint64)(&user2);
}

// 睡眠函数
void uartsleep(int sec) {
	// 1000000 * 10 * 2 同样是2s，为什么这个就少？
	// 可见这里通过自加的方式来计时十分不准确
	// qemu使用这种方式是，因为自增是QEMU实现的，所以相对比较准确
	int interval = 1000000 * 10 * sec;
	int count = 0;
	while (count++ < interval) {
		
	}
}
