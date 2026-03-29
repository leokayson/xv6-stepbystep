#include "proc.h"
#include "types.h"
#include "syscall.h"
#include "printf.h"
#include "uart.h"

uint64 systest();

uint64 (*syscalls[])(void) = {
    [SYS_test] = systest,
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