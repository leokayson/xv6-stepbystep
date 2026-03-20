#include "defs.h"
#include "riscv.h"
#include "proc.h"
#include "types.h"

extern struct process proc[];

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
    
    uartputs("=== Init Disk IO ===\n");
    virtio_disk_init();

    uartputs("=== Init Process ===\n");
    procinit();
    userinit();
    userinit1();

    __sync_synchronize();
    scheduler();

    while(1) {

    }

    // // 切换到第一个进程中，并进入到User modexia
    // w_sepc(proc[0].context.pc);
    // asm volatile("mv sp, %0" :: "r"(proc[0].context.sp));
    // w_sscratch((uint64)(&proc[0].context));
    
    // /* 在S mode上可以响应中断 */
    // // w_sstatus(r_sstatus() | SSTATUS_SIE);
    // // virtio_disk_test();

    // unsigned long x = r_sstatus();
    // x |= SSTATUS_SPP;   //set spp to 1 for supervisor mode
    // x |= SSTATUS_SPIE; // enable irqs in user mode
    // w_sstatus(x);

    // asm volatile("sret");

    // while (1) {

    // }
}
