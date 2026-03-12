#ifndef __DEFS_H__
#define __DEFS_H__

// uart.c
void uartinit();
void uartputs(char *s);

// kernelvec.c
void kernelvec();
void timervec();  // M mode下中断的入口

// trap.c
void trapinithart();
void kernelvec(); // in kernelvec.S, calls kerneltrap().
int devinitr();

#endif // !__DEFS_H__