# Initial process that execs /init.
# This code runs in user space.

.globl start
start1:
        la a6, filename # 加载地址到a0寄存器
        mv a0, a6
        li a1, 0x202    # 创建
        li a7, 15       # 系统调用sys_open -> 15
        ecall

        mv a5, a0       # 文件对象就在a0中
        la a6, filecontext
        mv a1, a6
        li a2, 6        # 6个字符
        li a7, 16       # 系统调用sys_write -> 16
        ecall

        mv a0, a5
        li a7, 21       # 系统调用sys_close -> 16
        ecall

loop:
        mv a0, a5
        li a1, 85
        j loop

filename:
  .string "/a.txt\0\0"
filecontext:
  .string "aaaaaa\0"
