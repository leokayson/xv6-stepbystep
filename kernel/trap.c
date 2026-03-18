#include "proc.h"
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"

uint32				  g_switch = 1; // 进程切换标志位
extern struct process proc[]; // defined in proc.c

// in kernelvec.S, calls kerneltrap().
void kernelvec();

/* 设置stvec寄存器 */
void trapinithart()
{
	w_stvec((uint64)kernelvec);
	// 临时修改中断入口
	w_stvec((uint64)kernelvec1);
}

/* 中断入口in S MODE */
// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap()
{
	uint64 sepc		 = r_sepc();
	uint64 sstatus	 = r_sstatus();
	uint64 scause	 = r_scause();
	int	   which_dev = 0;

	if ((sstatus & SSTATUS_SPP) == 0)
		panic("kerneltrap: not from supervisor mode");
	if (intr_get() != 0)
		panic("kerneltrap: interrupts enabled");

	which_dev = devintr();
	switch (which_dev) {
		case (0): {
			printf("scause 0x%lx\n", scause);
			printf("sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
			panic("kerneltrap");
			break;
		}
		case (2): {
			uartputs("Timer IRQ...\n");
			break;
		}
		case (1): {
			// irq indicates which device interrupted.
			int irq = plic_claim();

			if (irq == UART0_IRQ) {
				printf("UART IRQ\n");
				uartintr();
				printf("\n\n");
			} else if (irq == VIRTIO0_IRQ) {
				printf("VIRTIO disk IRQ\n");
				virtio_disk_intr();
			} else if (irq) {
				printf("unexpected interrupt irq=%d\n", irq);
			}

			// the PLIC allows each device to raise at most one
			// interrupt at a time; tell the PLIC the device is
			// now allowed to interrupt again.
			if (irq) {
				plic_complete(irq);
			}
		}
	}

	// give up the CPU if this is a timer interrupt.
	//   if(which_dev == 2 && myproc() != 0)
	//     yield();

	// the yield() may have caused some traps to occur,
	// so restore trap registers for use by kernelvec.S's sepc instruction.
	w_sepc(sepc);
	w_sstatus(sstatus);
}

// 临时修改，中断入口函数
uint64 kerneltrap1(uint64 epc)
{
	int	   which_dev = 0;
	uint64 return_pc = epc;

	which_dev = devintr();
	switch (which_dev) {
		case (0): {
			uint64 scause = r_scause();
			uartputs("panic: irq not handled\n");
			printf("scause = 0x%lx, spec = 0x%lx, stval = 0x%lx\n", scause, r_sepc(), r_stval());
			while (1) {
			}
			break;
		}
		case (2): {
			uartputs("Timer IRQ...\n");

			// 一个时间片然后进程切换
			struct context *next;

			next	  = &(proc[g_switch % 2].context);
			return_pc = next->pc;
			asm volatile("csrw sscratch, %0" ::"r"(next));
			g_switch++;
			break;
		}
		case (1): {
			uartputs("External IRQ...\n");
			int irq = plic_claim();

			printf("EXT_IRQ = %d\n", irq);
			if (irq == UART0_IRQ) {
				printf("EXT_IRQ_UART0\n");
				uartintr();
				printf("\n\n");
			} else if (irq == VIRTIO0_IRQ) {
				printf("VIRTIO disk IRQ\n");
				virtio_disk_intr();
			} else if (irq) {
				printf("unexpected interrupt irq=%d\n", irq);
			}

			// the PLIC allows each device to raise at most one
			// interrupt at a time; tell the PLIC the device is
			// now allowed to interrupt again.
			if (irq) {
				plic_complete(irq);
			}
		}
	}

	return return_pc;
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
