#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"

// in kernelvec.S, calls kerneltrap().
void kernelvec();

/* 设置stvec寄存器 */
void trapinithart()
{
	w_stvec((uint64)kernelvec);
}

/* 中断入口in S MODE */
// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
    uint64 sepc		 = r_sepc();
	uint64 sstatus	 = r_sstatus();
	uint64 scause	 = r_scause();
	int	   which_dev = devintr();

	if ((sstatus & SSTATUS_SPP) == 0)
		panic("kerneltrap: not from supervisor mode");
	if (intr_get() != 0)
		panic("kerneltrap: interrupts enabled");

	if (which_dev == 0) {
		printf("scause 0x%lx\n", scause);
		printf("sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
		panic("kerneltrap");
	} else if (which_dev == 2) {
        uartputs("Timer IRQ...\n");
    }

	// give up the CPU if this is a timer interrupt.
	//   if(which_dev == 2 && myproc() != 0)
	//     yield();

	// the yield() may have caused some traps to occur,
	// so restore trap registers for use by kernelvec.S's sepc instruction.
	w_sepc(sepc);
	w_sstatus(sstatus);
}

/* 获取中断异常原因 */
// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr()
{
	uint64 scause = r_scause();

	if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
		// this is a supervisor external interrupt, via PLIC.

		// irq indicates which device interrupted.
		int irq = plic_claim();

		if (irq == UART0_IRQ) {
			printf("UART IRQ\n");
			uartintr();
		} else if (irq == VIRTIO0_IRQ) {
			// virtio_disk_intr();
		} else if (irq) {
			printf("unexpected interrupt irq=%d\n", irq);
		}

		// the PLIC allows each device to raise at most one
		// interrupt at a time; tell the PLIC the device is
		// now allowed to interrupt again.
		if (irq)
			plic_complete(irq);

		return 1;
	} else if (scause == 0x8000000000000001L) {
		// software interrupt from a machine-mode timer interrupt,
		// forwarded by timervec in kernelvec.S.

		if (cpuid() == 0) {
			// clockintr();
		}

		// acknowledge the software interrupt by clearing
		// the SSIP bit in sip.
		w_sip(r_sip() & ~2);

		return 2;
	} else {
		return 0;
	}
}
