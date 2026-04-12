#ifndef __VM_H__
#define __VM_H__

#include "types.h"
#include "riscv.h"

void kvminit(void);
void kvminithart(void);
void vmprint_kernel(); // only for test
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
int	 mappages(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);

pagetable_t uvmcreate();
void		uvmfree(pagetable_t pagetable, uint64 sz);
void		uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, bool_t do_free);
int			uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
uint64		uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64		uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
void		uvmclear(pagetable_t pagetable, uint64 va);
void		uvmfirst(pagetable_t pagetable, uchar *src, uint sz);

void   freewalk(pagetable_t pagetable);
uint64 walkaddr(pagetable_t pagetable, uint64 va);

/*
    1. 用户地址为虚拟地址，因为已经开启分页，找到物理内存地址再进行拷贝；
    2. 内核对等映射，虚拟地址 = 物理地址
*/
// 内核 -> 用户va
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
// 用户va -> 内核
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
// 拷贝用户空间字符串
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);

#endif // !__VM_H__
