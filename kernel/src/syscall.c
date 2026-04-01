#include "proc.h"
#include "types.h"
#include "syscall.h"
#include "printf.h"
#include "uart.h"
#include "vm.h"
#include "string.h"

#define GET_TRAPFRAME(n)    p->trapframe->a##n

static uint64 argraw(int n);
void		  argint(int n, int *ip);
void		  argaddr(int n, uint64 *ip);
int			  fetchaddr(uint64 addr, uint64 *ip);
int			  fetchstr(uint64 addr, char *buf, int max);
int			  argstr(int n, char *buf, int max);

// 系统调用函数原型
extern uint64 sys_fork();
extern uint64 sys_exit();
extern uint64 sys_wait();
extern uint64 sys_pipe();
extern uint64 sys_read();
extern uint64 sys_kill();
extern uint64 sys_exec();
extern uint64 sys_fstat();
extern uint64 sys_chdir();
extern uint64 sys_dup();
extern uint64 sys_getpid();
extern uint64 sys_sbrk();
extern uint64 sys_sleep();
extern uint64 sys_uptime();
extern uint64 sys_open();
extern uint64 sys_write();
extern uint64 sys_mknod();
extern uint64 sys_unlink();
extern uint64 sys_link();
extern uint64 sys_mkdir();
extern uint64 sys_close();
uint64		  systest();

uint64 (*syscalls[])(void) = {
    [SYS_fork]          = sys_fork,
    [SYS_exit]          = sys_exit,
    [SYS_wait]          = sys_wait,
    [SYS_pipe]          = sys_pipe,
    [SYS_read]          = sys_read,
    [SYS_kill]          = sys_kill,
    [SYS_exec]          = sys_exec,
    [SYS_fstat]         = sys_fstat,
    [SYS_chdir]         = sys_chdir,
    [SYS_dup]           = sys_dup,
    [SYS_getpid]        = sys_getpid,
    [SYS_sbrk]          = sys_sbrk,
    [SYS_sleep]         = sys_sleep,
    [SYS_uptime]        = sys_uptime,
    [SYS_open]          = sys_open,
    [SYS_write]         = sys_write,
    [SYS_mknod]         = sys_mknod,
    [SYS_unlink]        = sys_unlink,
    [SYS_link]          = sys_link,
    [SYS_mkdir]         = sys_mkdir,
    [SYS_close]         = sys_close,
    [SYS_test]          = systest,
};

void syscall() {
    int num;
    struct process* p = myproc();
    num = p->trapframe->a7;

    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("[%d]%s: unknwon syscall %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }

    uartsleep(20);
}

uint64 systest() {
    printf("This is systest\n");

    return SYS_test;
}

static uint64 argraw(int n) {
    struct process *p = myproc();
    switch (n) {
        case 0:
            return GET_TRAPFRAME(0);
        case 1:
            return GET_TRAPFRAME(1);
        case 2:
            return GET_TRAPFRAME(2);
        case 3:
            return GET_TRAPFRAME(3);
        case 4:
            return GET_TRAPFRAME(4);
        case 5:
            return GET_TRAPFRAME(5);
    }
    panic("argraw");
    return -1;
}

// 获取32位的系统调用过程参数值，第n个int *类型
void argint(int n, int *ip) {
    *ip = argraw(n);
}

// 获取64位的系统调用参数值，第n个int64 *类型
void argaddr(int n, uint64 *ip) {
    *ip = argraw(n);
}

// 获取用户进程地址中的整数值
int fetchaddr(uint64 addr, uint64 *ip) {
    struct process *p = myproc();
    if (addr >= p->sz || addr + sizeof(uint64) > p->sz) {
        return -1; // 非法
    }

    return copyin(p->pagetable, (char *)ip, addr, sizeof(*ip));
}

// 获取用户进程地址中的字符串
int fetchstr(uint64 addr, char *buf, int max) {
    struct process *p = myproc();
    
    if (copyinstr(p->pagetable, buf, addr, max) < 0) {
        return -1;
    }

    return  strlen(buf);
}


// 获取用户进程中第n个参数指向的字符串
int argstr(int n, char *buf, int max) {
    uint64 addr;
    argaddr(n, &addr);

    return fetchstr(addr, buf, max);
}