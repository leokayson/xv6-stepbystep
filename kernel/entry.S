.section .text

.global _entry
_entry:
    la sp, stack0           # 加载地址到寄存器
    li a0, 1024*4           # 加载常量到寄存器
    csrr a1, mhartid        # 读取硬件线程id
    addi a1, a1, 1          # a1 = mhartid + 1
    mul a0, a0, a1          # a0 = (1024*4) + (mhartid + 1)
    add sp, sp, a0          # 在sp分配空间

    call start              # 跳转到start函数中
spin:
    j spin