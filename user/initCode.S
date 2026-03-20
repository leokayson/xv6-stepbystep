# Initial process that execs /init.
# This code runs in user space.

#include "syscall.h"

.globl start
start:
        la a0, 1
        la a1, 2
        la a2, 0
        add a2, a0, a1
        j start

init:
  .string "/init\0"

.globl start1
start1:
        la a0, 2
        la a1, 3
        la a2, 0
        add a2, a0, a1
        j start

init1:
  .string "/init\0"
