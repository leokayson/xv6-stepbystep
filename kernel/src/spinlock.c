// Mutual exclusion spin locks.
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "printf.h"

void initlock(struct spinlock *lk, char *name)
{
	lk->name   = name;
	lk->locked = 0;
	lk->cpu	   = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void acquire(struct spinlock *lk)
{
	critical_enter(); // disable interrupts to avoid deadlock.
	if (holding(lk))
		panic("re-acquire");

	// On RISC-V, __sync_lock_test_and_set turns into an atomic swap:
	//   a5 = 1
	//   s1 = &lk->locked
	//   amoswap.w.aq a5, a5, (s1)
	while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
		;

	// Tell the C compiler and the processor to not move loads or stores
	// past this point, to ensure that the critical section's memory
	// references happen strictly after the lock is acquired.
	// On RISC-V, this emits a fence instruction.
	__sync_synchronize();

	// Record info about lock acquisition for holding() and debugging.
	lk->cpu = mycpu();
}

// Release the lock.
void release(struct spinlock *lk)
{
	if (!holding(lk))
		panic("re-release");

	lk->cpu = 0;

	// Tell the C compiler and the CPU to not move loads or stores
	// past this point, to ensure that all the stores in the critical
	// section are visible to other CPUs before the lock is released,
	// and that loads in the critical section occur strictly before
	// the lock is released.
	// On RISC-V, this emits a fence instruction.
	__sync_synchronize();

	// Release the lock, equivalent to lk->locked = 0.
	// This code doesn't use a C assignment, since the C standard
	// implies that an assignment might be implemented with
	// multiple store instructions.
	// On RISC-V, sync_lock_release turns into an atomic swap:
	//   s1 = &lk->locked
	//   amoswap.w zero, zero, (s1)
	__sync_lock_release(&lk->locked);

	critical_exit();
}

// Check whether this cpu is holding the lock.
// Interrupts must be off.
int holding(struct spinlock *lk)
{
	return (lk->locked && lk->cpu == mycpu());
}

// critical_enter/critical_exit are like intr_off()/intr_on() except that they are matched:
// it takes two critical_exit()s to undo two critical_enter()s.  Also, if interrupts
// are initially off, then critical_enter, critical_exit leaves them off.

void critical_enter(void)
{
	int enabled = intr_get();

	intr_off();
	if (mycpu()->nesting == 0)
		mycpu()->intr_is_enabled = enabled;
	mycpu()->nesting += 1;
}

void critical_exit(void)
{
	struct cpu *c = mycpu();
	if (intr_get())
		panic("critical_exit - interruptible");
	if (c->nesting < 1)
		panic("critical_exit");
	c->nesting -= 1;
	if (c->nesting == 0 && c->intr_is_enabled)
		intr_on();
}