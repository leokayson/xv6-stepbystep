#include "param.h"
#include "types.h"
#include "riscv.h"
#include "memlayout.h"

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void main();
void timerinit();
void timervec();
uint64 timer_scratch[NCPU][5];

void start() {
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
    
    w_mstatus(mstatus);

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

    // 时钟中断初始化
    timerinit();
    
    // keep each CPU's hartid in its tp register, for cpuid().
    int id = r_mhartid();
    // Thread Pointer
    w_tp(id);

    asm volatile("mret");
}

void timerinit() {
    /*
     * 1. 读取硬件线程ID
     * 2. 设置mtimecmp寄存器。配置中断间隔时间
     * 3. 设置msratch寄存器。中断处理逻辑
     * 4. 设置M模式下，中断处理入口寄存器，即 mtvec 寄存器
     * （由于此时M mode下，只处理时钟中断，其他的中断将被S mode处理）目前sscratch寄存器在trampoline.S中设置
     * 5. 使能M mode下的中断
     * 6. 使能M mode下的时钟中断
    */
    // each CPU has a separate source of timer interrupts.
    uint64 id = r_mhartid();
    int cycle_100ms = 1000000; 
    int interval = cycle_100ms * 20;

    // ask the CLINT for a timer interrupt.
    *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

    // prepare information in scratch[] for timervec.
    // scratch[0..2] : space for timervec to save registers.
    // scratch[3] : address of CLINT MTIMECMP register.
    // scratch[4] : desired interval (in cycles) between timer interrupts.
    uint64 *scratch = &timer_scratch[id][0];
    scratch[3] = CLINT_MTIMECMP(id);
    scratch[4] = interval;
    w_mscratch((uint64)scratch);

    // set the machine-mode trap handler.
    w_mtvec((uint64)timervec);

    // enable machine-mode interrupts.
    w_mstatus(r_mstatus() | MSTATUS_MIE);

    // enable machine-mode timer interrupts.
    w_mie(r_mie() | MIE_MTIE);
}
