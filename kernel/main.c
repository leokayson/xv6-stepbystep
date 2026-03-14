#include "defs.h"
#include "riscv.h"

void main() {
    uartputs("=== Enter main ===\n");
    uartputs("=== Init UART ===\n");
    uartinit();
    uartputs("=== Init trap ===\n");
    trapinithart();
    
    /* 在S mode上可以响应中断 */
    w_sstatus(r_sstatus() | SSTATUS_SIE);
    
    uartputs("Hello xv6\n");
    
    uartputs("=== Init physical memory ===\n");
    kinit();
    kmemtest();
    uartputs("=== Init virtual memory ===\n");
    kvminit();
    // kvminithart();

    vmprint_kernel();
    
    while (1) {

    }
}
