#ifndef __DEFS_H__
#define __DEFS_H__

#include "types.h"

// uart.c
void uartinit();
void uartputs(char *s);
void uartputc(char c);
void consputc(char c);
int uartgetc();
void uartintr();

// kernelvec.c
void kernelvec();
void timervec(); // M mode下中断的入口

// trap.c
void trapinithart();
void kernelvec(); // in kernelvec.S, calls kerneltrap().
int	 devintr();

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
void   kvminit(void);
void   kvminithart(void);
void   vmprint_kernel(); // only for test

struct cpu;
struct spinlock;

// proc.c
int cpuid();
struct cpu *mycpu();

// spinlock.c
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);
void critical_enter(void);
void critical_exit(void);

// plic.c
void            plicinit(void);
void            plicinithart(void);
int             plic_claim(void);
void            plic_complete(int irq);

#endif // !__DEFS_H__
