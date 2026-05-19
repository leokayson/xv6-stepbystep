#ifndef __TRAP_H__
#define __TRAP_H__
#include "spinlock.h"
#include "types.h"

void trapinithart();
void kernelvec(); // in kernelvec.S, calls kerneltrap().
int	 devintr();

void usertrap();
void usertrapret();

void				   clockintr();
extern uint			   ticks;
extern struct spinlock tickslock;

#endif // !__TRAP_H__
