#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "defs.h"

struct spinlock {
	uint		locked;
	char	   *name;
	struct cpu *cpu;
};

#endif // !__SPINLOCK_H__