#include "memlayout.h"
#include "defs.h"
#include "riscv.h"
#include "types.h"
#include "spinlock.h"

extern char end[]; // kernel.ld 中定义，内核镜像结束地址

struct run {
	struct run *next;
};

struct {
	struct run *freelist; // 链表头
	struct spinlock lock;
} kmem;

void freerange(void *pa_start, void *pa_end);

// 空闲内存地址的初始化
void kinit(void)
{
	initlock(&kmem.lock, "kmem");
	freerange(end, (void *)PHYSTOP);
}

// 分配物理内存地址
void *kalloc(void)
{
	struct run *r;

	acquire(&kmem.lock);
	r = kmem.freelist;
	if (r)
		kmem.freelist = r->next;
	release(&kmem.lock);

	if (r)
		memset((char *)r, 5, PGSIZE); // fill with junk
	return (void *)r;
}

// 将内存地址放到链表中
void kfree(void *pa)
{
	struct run *r;

	if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
		panic("kfree");

	// Fill with junk to catch dangling refs.
	memset(pa, 1, PGSIZE);

	r = (struct run *)pa;

	acquire(&kmem.lock);
	r->next		  = kmem.freelist;
	kmem.freelist = r;
	release(&kmem.lock);
}

// 针对物理内存一段范围进行初始化
void freerange(void *pa_start, void *pa_end)
{
	char *p;
	p = (char *)PGROUNDUP((uint64)pa_start);
	for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
		kfree(p);
}

void kmemtest() {
	uint64 *p1 = (uint64 *)kalloc();
	if (p1) {
		*p1 = 0x6;
		printf("p1 = %p\n", p1);
	}
	uint64 *p2 = (uint64 *)kalloc();
	if (p2) {
		*p2 = 0x6;
		printf("p2 = %p\n", p2);
	}

	kfree(p1);
	kfree(p2);
	uint64 *p3 = (uint64 *)kalloc();
	if (p3) {
		*p3 = 0x6;
		printf("p3 = %p\n\n", p3);
	}

	kfree(p3);

	p1 = (uint64 *)kalloc();
	if (p1) {
		*p1 = 0x6;
		printf("p1 = %p\n", p1);
	}
	p2 = (uint64 *)kalloc();
	if (p2) {
		*p2 = 0x6;
		printf("p2 = %p\n", p2);
	}

	kfree(p2);
	kfree(p1);
	p3 = (uint64 *)kalloc();
	if (p3) {
		*p3 = 0x6;
		printf("p3 = %p\n\n", p3);
	}
	
	kfree(p3);
}
