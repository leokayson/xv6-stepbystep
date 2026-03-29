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
void		uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, bool do_free);
void		freewalk(pagetable_t pagetable);
void		uvmfirst(pagetable_t pagetable, uchar *src, uint sz);

#endif // !__VM_H__
