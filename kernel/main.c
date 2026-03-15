#include "defs.h"
#include "riscv.h"

void main() {
    uartputs("=== Enter main ===\n");
    uartputs("=== Init UART ===\n");
    uartinit();
    uartputs("=== Init trap ===\n");
    trapinithart();
    
    uartputs("Hello xv6\n");
    
    uartputs("=== Init physical memory ===\n");
    kinit();
    kmemtest();

    uartputs("=== Init virtual memory ===\n");
    kvminit();
    kvminithart();
    // vmprint_kernel();
    
    uartputs("=== Init PLIC ===\n");
    plicinit();
    plicinithart();
    
    /* 在S mode上可以响应中断 */
    w_sstatus(r_sstatus() | SSTATUS_SIE);
    
    uartputs("=== Init Disk IO ===\n");
    virtio_disk_init();
    virtio_disk_test();
    
    while (1) {

    }
}
