#include "proc.h"
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "memlayout.h"

uint32				  g_switch = 1; // 进程切换标志位
extern struct process proc[]; // defined in proc.c
extern char			  uservec[];
extern char			  userret[];
extern char			  trampoline[];

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
	int	   which_dev = 0;

	if ((sstatus & SSTATUS_SPP) == 0)
		panic("kerneltrap: not from supervisor mode");
	if (intr_get() != 0)
		panic("kerneltrap: interrupts enabled");

	printf("[M] ");
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
			
			uartputs("PLIC IRQ\n");

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

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void)
{
	int which_dev = 0;

	if ((r_sstatus() & SSTATUS_SPP) != 0)
		panic("usertrap: not from user mode");

	printf("[U] ");

	// send interrupts and exceptions to kerneltrap(),
	// since we're now in the kernel.
	w_stvec((uint64)kernelvec);

	struct process *p = myproc();
	which_dev = devintr();

	// save user program counter.
	p->trapframe->epc = r_sepc();

	/*
		8 -> Environment call from U-Mode
		9 -> Environment call from S-Mode
		11 -> Environment call from M-Mode
	*/
	if (r_scause() == 8) {
		printf("System call\n");
		// system call

		// if (killed(p))
		// 	exit(-1);

		// sepc points to the ecall instruction,
		// but we want to return to the next instruction.
		p->trapframe->epc += 4; // 执行epc寄存器下一条指令，必须修改中断入口

		// 暂时注释
		// 后续打开，防止系统调用长时间执行，无法相应设备中断，导致系统卡顿现象
		// intr_on();

		syscall();
	} else if (which_dev != 0) {
		if (which_dev == 1) {
			printf("PLIC IRq\n");
		} else if (which_dev == 2) {
			printf("Timer IRQ\n");
		} else {
			printf("Other IRQ\n");
		}
		// ok
	} else {
		printf("Unexpected scause\n");
		printf("\tscause = 0x%lx, pid = %d\n", r_scause(), p->pid);
		printf("\tsepc = 0x%lx, stval=0x%lx\n", r_sepc(), r_stval());
		// setkilled(p);
	}
	
	// if (killed(p))
	// 	exit(-1);
	
	// give up the CPU if this is a timer interrupt.
	if (which_dev == 2) {
		trace_next_pid("Timer");
		yield();
	}

	usertrapret();
}

//
// return to user space
//
void usertrapret(void)
{
	struct process *p = myproc();

	// we're about to switch the destination of traps from
	// kerneltrap() to usertrap(), so turn off interrupts until
	// we're back in user space, where usertrap() is correct.
	intr_off();

	// send syscalls, interrupts, and exceptions to uservec in trampoline.S
	uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
	w_stvec(trampoline_uservec);

	// set up trapframe values that uservec will need when
	// the process next traps into the kernel.
	p->trapframe->kernel_satp	= r_satp(); // kernel page table
	p->trapframe->kernel_sp		= p->kstack + PGSIZE; // process's kernel stack
	p->trapframe->kernel_trap	= (uint64)usertrap;
	p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

	// set S Previous Privilege mode to User.
	unsigned long x = r_sstatus();
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE; // enable interrupts in user mode
	w_sstatus(x);

	// set S Exception Program Counter to the saved user pc.
	w_sepc(p->trapframe->epc);

	// tell trampoline.S the user page table to switch to.
	uint64 satp = MAKE_SATP(p->pagetable);

	// jump to userret in trampoline.S at the top of memory, which
	// switches to the user page table, restores user registers,
	// and switches to user mode with sret.
	uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
	((void (*)(uint64))trampoline_userret)(satp);
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
