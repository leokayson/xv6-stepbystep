#include "types.h"
#include "riscv.h"
#include "defs.h"

// in kernelvec.S, calls kerneltrap().
void kernelvec();

/* 设置stvec寄存器 */
void trapinithart() {
    w_stvec((uint64)kernelvec);
}

/* 中断入口in S MODE */
void kerneltrap() {
    int which_dev = devinitr();

    if (which_dev == 0) {
        uartputs("panic: irq not hanled\n");
        while (1) ;
    } else if (which_dev == 2) {
        uartputs("timer irq...\n");
    } else {
        uartputs("panic: irq not hanled\n");
        while (1) ;
    }
}

/* 获取中断异常原因 */
int devinitr() {
    uint64 scause = r_scause();

    if (scause == 0x8000000000000001L) {/* 31bit Interruput, 9: S mode external irq */
        // this is a supervisor external interrupt, via PLIC.
        // irq indicates which device interrupted.

        w_sip(r_sip() & ~2);  // clear sw irq
        return 2;
    }
    return 0;
}