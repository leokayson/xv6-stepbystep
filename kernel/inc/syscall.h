#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "types.h"

// System call numbers
#define SYS_fork   1
#define SYS_exit   2
#define SYS_wait   3
#define SYS_pipe   4
#define SYS_read   5
#define SYS_kill   6
#define SYS_exec   7
#define SYS_fstat  8
#define SYS_chdir  9
#define SYS_dup	   10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_test   22

#define NELEM(x) (sizeof(x) / sizeof(x[0]))

uint64 systest();
void   syscall();

void argint(int n, int *ip);
void argaddr(int n, uint64 *ip);
int fetchaddr(uint64 addr, uint64 *ip);
int fetchstr(uint64 addr, char *buf, int max);
int argstr(int n, char *buf, int max);

#endif // !__SYSCALL_H__
