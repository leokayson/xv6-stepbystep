#ifndef __DEFS_H__
#define __DEFS_H__

#include "types.h"
#include "riscv.h"

// uart.c
void uartinit();
void uartputs(char *s);
void uartputc(char c);
void consputc(char c);

// kernelvec.c
void kernelvec();
void timervec(); // M mode下中断的入口

// trap.c
void trapinithart();
void kernelvec(); // in kernelvec.S, calls kerneltrap().
int	 devinitr();

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
#endif // !__DEFS_H__
