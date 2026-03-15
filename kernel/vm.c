#include "defs.h"
#include "types.h"
#include "riscv.h"
#include "memlayout.h"

// 内核页表
pagetable_t kernel_page_table;
extern char etext[]; // defined in kernel.ld

pagetable_t kvmmake(void);
void   kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
pte_t *walk(pagetable_t kpgtbl, uint64 va, int alloc);

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
	kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_RW);
	kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_RW);
	kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_RX);
	/* map kernel data and the physical RAM we'll make use of */
	kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_RW);
	// map the trampoline page to the highest address in both user and kernel space.
	// kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_RX);

	// allocate and map a kernel stack for each process
	// proc_mapstacks(kpgtbl);

	return kpgtbl;
}

// 映射函数
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
	uint64 a, last;
	pte_t *pte;

	if (va % PGSIZE) {
		panic("va not aligned");
	}

	if (sz == 0) {
		panic("size == 0");
	}

	if (sz % PGSIZE) {
		panic("size not aligned");
	}

	a	 = va;
	last = va + sz - PGSIZE;

	while (1) {
		if (((pte) = walk(kpgtbl, a, 1)) == 0) {
			panic("no enough page");
		}
		if (*pte & PTE_V) {
			panic("remmaped");
		}
		*pte = PA2PTE(pa) | perm | PTE_V;
		if (a == last) {
			break;
		}
		a += PGSIZE;
		pa += PGSIZE;
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