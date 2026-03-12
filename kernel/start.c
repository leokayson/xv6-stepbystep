#define NCPU 8

#include "types.h"
#include "riscv.h"

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];



void start() {
    extern void main();
    // Jump to main
    /*
        1. 设置mret返回值的模式为S模式；
        2. 设置mret的返回地址；
        3. 关闭地址转换和保护
        4. 在S模式下，代理所有中断和异常；
        5. 允许S模式访问所有物理内存；
        6. 时间中断初始化；
        7. 执行mret指令
    */

    uint64 mstatus = r_mstatus();
    
    // set M Previous Privilege mode to Supervisor, for mret.
    mstatus &= ~MSTATUS_MPP_MASK; /* 清空之前的模式  */ 
    mstatus |= MSTATUS_MPP_S;
    
    // 关闭地址转换和保护
    w_satp(0);
    // 设置mret的返回地址
    w_mepc((uint64)main);

    // 代理所有中断和异常到S模式下
    // Machine Exception Delegation
    // Machine Interrupt Delegation
    w_medeleg(0xffff);
    w_mideleg(0xffff);

    // external timer softare
    // sie = Supervisor Interrupt Enable
    // SIE_SEIE = S-mode External Interrupt Enable
    // SIE_STIE = S-mode Timer Interrupt Enable
    // SIE_SSIE = S-mode Softare Interrupt Enable
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    // TODO 时钟中断初始化
    
    // keep each CPU's hartid in its tp register, for cpuid().
    int id = r_mhartid();
    // Thread Pointer
    w_tp(id);

    asm volatile("mret");
}
