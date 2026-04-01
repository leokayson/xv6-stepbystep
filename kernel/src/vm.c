#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "printf.h"
#include "kalloc.h"
#include "string.h"
#include "vm.h"
#include "proc.h"
#include "kalloc.h"

// 内核页表
pagetable_t kernel_page_table;
extern char etext[]; // defined in kernel.ld
extern char trampoline[]; // defined in trampoline.S

pagetable_t kvmmake(void);
pte_t	   *walk(pagetable_t kpgtbl, uint64 va, int alloc);

// 初始化内核页表
void kvminit()
{
	kernel_page_table = kvmmake();
}

// 启用内核页表
void kvminithart()
{
	// wait for any previous writes to the page table memory to finish.
	sfence_vma();

	w_satp(MAKE_SATP(kernel_page_table));

	// flush stable entries from the TLB.
	sfence_vma();
}

// 添加映射，将对应的内存进行映射，xv6使用对等映射，虚拟地址和物理地址一样
pagetable_t kvmmake(void)
{
	pagetable_t kpgtbl;

	kpgtbl = (pagetable_t)kalloc();
	if (!kpgtbl) {
		panic("kvmmake kalloc fiailed");
	}
	// 创建0号level3页表
	memset(kpgtbl, 0, PGSIZE);

	kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_RW);
	kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_RW);
	kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_RW);
	kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_RX);
	/* map kernel data and the physical RAM we'll make use of */
	kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_RW);
	// map the trampoline page to the highest address in both user and kernel space.
	kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_RX);

	// allocate and map a kernel stack for each process
	proc_mapstacks(kpgtbl);

	return kpgtbl;
}

// 映射函数
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
	if (mappages(kpgtbl, va, pa, sz, perm)) {
		panic("no enough page");
	}
}

// 返回页表中va对应pte地址，页表项page table entry
// 若alloc为0, 则不创建页表项，否则创建页表项
/*
    rv39
    39..64 0
    30..38 9 bit of level-2 index
    21..29 9 bit of level-1 index
    12..20 9 bit of level-0 index
    0..11 12 bit of byte offset within the page
*/
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc)
{
	if (va >= MAXVA) {
		panic("walk to maxVA");
	}

	for (int level = 2; level > 0; level--) {
		pte_t *pte = &pagetable[PX(level, va)];

		if (*pte & PTE_V) {
			pagetable = (pagetable_t)PTE2PA(*pte);
		} else {
			if (!alloc || (pagetable = (pde_t *)kalloc()) == 0) {
				return 0;
			}
			memset(pagetable, 0, PGSIZE);
			*pte = PA2PTE(pagetable) | PTE_V;
		}
	}
	return &pagetable[PX(0, va)];
}

// 创建用户页表，未映射
pagetable_t uvmcreate()
{
	pagetable_t pagetable = (pagetable_t)kalloc();

	if (pagetable == 0) {
		return 0;
	}

	memset(pagetable, 0, PGSIZE);
	return pagetable;
}

// Free user memory pages,
// then free page-table pages.
void uvmfree(pagetable_t pagetable, uint64 sz)
{
	if (sz > 0)
		uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
	freewalk(pagetable);
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, bool_t do_free)
{
	uint64 a;
	pte_t *pte;

	if ((va % PGSIZE) != 0)
		panic("uvmunmap: not aligned");

	for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
		if ((pte = walk(pagetable, a, 0)) == 0)
			panic("uvmunmap: walk");
		if ((*pte & PTE_V) == 0)
			panic("uvmunmap: not mapped");
		if (PTE_FLAGS(*pte) == PTE_V)
			panic("uvmunmap: not a leaf");
		if (do_free) {
			uint64 pa = PTE2PA(*pte);
			kfree((void *)pa);
		}
		*pte = 0;
	}
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void freewalk(pagetable_t pagetable)
{
	// there are 2^9 = 512 PTEs in a page table.
	for (int i = 0; i < 512; i++) {
		pte_t pte = pagetable[i];
		if (pte & PTE_V) {
			if ((pte & (PTE_RWX)) != 0) {
				panic("freewalk: leaf");
			}

			// this PTE points to a lower-level page table.
			uint64 child = PTE2PA(pte);
			freewalk((pagetable_t)child);
			pagetable[i] = 0;
		}
	}
	kfree((void *)pagetable);
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
	uint64 a, last;
	pte_t *pte;

	if ((va % PGSIZE) != 0)
		panic("mappages: va not aligned");

	if ((sz % PGSIZE) != 0)
		panic("mappages: size not aligned");

	if (sz == 0)
		panic("mappages: size");

	a	 = va;
	last = va + sz - PGSIZE;
	for (;;) {
		if ((pte = walk(pagetable, a, 1)) == 0)
			return -1;
		if (*pte & PTE_V)
			panic("mappages: remap");
		*pte = PA2PTE(pa) | perm | PTE_V;
		if (a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
	char *mem;

	if (sz >= PGSIZE)
		panic("uvmfirst: more than a page");
	mem = kalloc();
	memset(mem, 0, PGSIZE);
	mappages(pagetable, 0, (uint64)mem, PGSIZE, PTE_RWX | PTE_U);
	memmove(mem, src, sz);
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64 walkaddr(pagetable_t pagetable, uint64 va)
{
	pte_t *pte;
	uint64 pa;

	if (va >= MAXVA)
		return 0;

	pte = walk(pagetable, va, 0);
	if (pte == 0)
		return 0;
	if ((*pte & PTE_V) == 0)
		return 0;
	if ((*pte & PTE_U) == 0)
		return 0;
	pa = PTE2PA(*pte);
	return pa;
}

// 内核 -> 用户va
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
	uint64 n, va0, pa0;

	while (len > 0) {
		va0 = PGROUNDDOWN(dstva);
		pa0 = walkaddr(pagetable, va0);
		if (pa0 == 0) {
			return -1;
		}
		n = PGSIZE - (dstva - va0);
		if (n > len) {
			n = len;
		}
		memmove((void *)(pa0 + (dstva - va0)), src, n);

		len -= n;
		src += n;
		dstva = va0 + PGSIZE;
	}
	return 0;
}

// 用户va -> 内核
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
	uint64 n, va0, pa0;

	while (len > 0) {
		va0 = PGROUNDDOWN(srcva);
		pa0 = walkaddr(pagetable, va0);
		if (pa0 == 0) {
			return -1;
		}
		n = PGSIZE - (srcva - va0);
		if (n > len) {
			n = len;
		}
		memmove(dst, (void *)(pa0 + (srcva - va0)), n);

		len -= n;
		dst += n;
		srcva = va0 + PGSIZE;
	}

	return 0;
}

// 拷贝用户空间字符串
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
	uint64 n, va0, pa0;
	int	   got_pull = 0;

	while (got_pull == 0 && max > 0) {
		va0 = PGROUNDDOWN(srcva);
		pa0 = walkaddr(pagetable, va0);
		if (pa0 == 0) {
			return -1;
		}
		n = PGSIZE - (srcva - va0);
		if (n > max) {
			n = max;
		}
		char *p = (void *)(pa0 + (srcva - va0));
		while (n > 0) {
			if (*p == '\0') {
				*dst	 = '\0';
				got_pull = 1;
				break;
			} else {
				*dst = *p;
			}
			--n;
			--max;
			p++;
			dst++;
		}

		srcva = va0 + PGSIZE;
	}

	return got_pull ? 0 : -1;
}

void _pteprint(pagetable_t pagetable, int level)
{
	for (int i = 0; i < 512; ++i) {
		pte_t pte = pagetable[i];

		if (pte & PTE_V) {
			for (int j = 0; j < level; j++) {
				printf(".. ");
			}
			printf("%d: pte, %p pa %p\n", i, (void *)pte, (void *)PTE2PA(pte));

			if ((pte & PTE_RWX) == 0) {
				// this PTE points to a lower-level page table
				uint64 child = PTE2PA(pte);
				_pteprint((pagetable_t)child, level + 1);
			}
		}
	}
}

void vmprint(pagetable_t pagetable)
{
	printf("page table %p\n", pagetable);
	_pteprint(pagetable, 0);
}

void vmprint_kernel()
{
	vmprint(kernel_page_table);
}
