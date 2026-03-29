#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "types.h"

struct spinlock {
	uint		locked;
	char	   *name;
	struct cpu *cpu;
};

void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int	 holding(struct spinlock *lk);
void critical_enter(void);
void critical_exit(void);

#endif // !__SPINLOCK_H__
