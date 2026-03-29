#ifndef __DEFS_H__
#define __DEFS_H__

#include "proc.h"

#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a < b) ? b : a)
#define NULL	  0

// kernelvec.S
void kernelvec();
void timervec(); // M mode下中断的入口

// swtch.S
void swtch(struct context *old, struct context *new);

#endif // !__DEFS_H__
