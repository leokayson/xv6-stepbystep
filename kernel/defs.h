#ifndef __DEFS_H__
#define __DEFS_H__

#include "proc.h"
#include "riscv.h"
#include "types.h"

struct cpu;
struct spinlock;
struct sleeplock;
struct buf;

struct context;
struct process;

// uart.c
void uartinit();
void uartputs(char *s);
void uartputc(char c);
void consputc(char c);
int	 uartgetc();
void uartintr();
void uartsleep(int sec);

// kernelvec.c
void kernelvec();
void timervec(); // M mode下中断的入口

// trap.c
void trapinithart();
void kernelvec(); // in kernelvec.S, calls kerneltrap().
int	 devintr();

void usertrap();
void usertrapret();

// kalloc.c
void *kalloc(void);
void  kfree(void *);
void  kinit(void);
void  kmemtest();

// string.c
int	  memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int	  strlen(const char *);
int	  strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

// printf.c
int	 printf(char *, ...) __attribute__((format(printf, 1, 2)));
void panic(char *) __attribute__((noreturn));
void printfinit(void);

// vm.c
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

// proc.c
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
void sleep(void* chan, struct spinlock *lk);
// 唤醒
void wakeup(void* chan);

// spinlock.c
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int	 holding(struct spinlock *lk);
void critical_enter(void);
void critical_exit(void);

// sleeplock.c
void initsleeplock(struct sleeplock *lk, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
bool is_holdingsleep(struct sleeplock *lk);

// plic.c
void plicinit(void);
void plicinithart(void);
int	 plic_claim(void);
void plic_complete(int irq);

// virtio_disk.c
void virtio_disk_init(void);
void virtio_disk_rw(struct buf *b, bool is_write);
void virtio_disk_intr(void);
void virtio_disk_test(void);

// swtch.S
void swtch(struct context *old, struct context *new);

// syscall.s
uint64 systest();
void   syscall();
#define NELEM(x) (sizeof(x) / sizeof(x[0]))

#endif // !__DEFS_H__
