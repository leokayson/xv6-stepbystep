# Initial process that execs /init.
# This code runs in user space.

#include "syscall.h"

# .globl start
# start:
#         li a0, 0x22
#         li a1, 0x33
#         li a7, 0x16 # 22: system call
#         ecall
        
#         li a0, 0x44
#         li a1, 0x55
#         j start

# init:
#   .string "/init\0"

.globl start1
start1:
        la a0, 2
        la a1, 3
        la a2, 0
        add a2, a0, a1
        j start

init1:
  .string "/init\0"

.globl start
start1:
        la a6, filename // 加载地址到a0寄存器
        mv a0, a6
        li a1, 0x202    // 创建
        li a7, 15       // 系统调用sys_open -> 15
        ecall

        mv a5, a0       // 文件对象就在a0中
        la a6, filecontext
        mv a1, a6
        li a2, 6        // 6个字符
        li a7, 16       // 系统调用sys_write -> 16
        ecall

        mv a0, a5
        li a7, 21       // 系统调用sys_close -> 16
        ecall

loop:
        li a0, a5
        li a1, 85
        j loop
filename:
  .string "/a.txt\0\0"
filecontext:
  .string "aaaaaa\0"
