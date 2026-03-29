#ifndef __TRAP_H__
#define __TRAP_H__

void trapinithart();
void kernelvec(); // in kernelvec.S, calls kerneltrap().
int	 devintr();

void usertrap();
void usertrapret();

#endif // !__TRAP_H__
