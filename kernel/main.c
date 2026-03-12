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
    
    uartputs("Hello World\n");

    while (1) {

    }
}
