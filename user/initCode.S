# Initial process that execs /init.
# This code runs in user space.

#include "syscall.h"

.globl start
start:
        li a0, 0x22
        li a1, 0x33
        li a7, 0x16 # 22: system call
        ecall
        
        li a0, 0x44
        li a1, 0x55
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
