        #
        # interrupts and exceptions while in supervisor
        # mode come here.
        #
        # the current stack is a kernel stack.
        # push registers, call kerneltrap().
        # when kerneltrap() returns, restore registers, return.
        #
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        # make room to save registers.
        addi sp, sp, -256

        # save caller-saved registers.
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        # call the C trap handler in trap.c
        call kerneltrap

        # restore registers.
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        # not tp (contains hartid), in case we moved CPUs
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        addi sp, sp, 256

        # return to whatever we were doing in the kernel.
        sret
######################
        .align 4
        .macro saveregs base
                sd ra, 0(\base)
                sd sp, 8(\base)
                sd gp, 16(\base)
                sd tp, 24(\base)
                sd t0, 32(\base)
                sd t1, 40(\base)
                sd t2, 48(\base)
                sd s0, 56(\base)
                sd s1, 64(\base)
                sd a0, 72(\base)
                sd a1, 80(\base)
                sd a2, 88(\base)
                sd a3, 96(\base)
                sd a4, 104(\base)
                sd a5, 112(\base)
                sd a6, 120(\base)
                sd a7, 128(\base)
                sd s2, 136(\base)
                sd s3, 144(\base)
                sd s4, 152(\base)
                sd s5, 160(\base)
                sd s6, 168(\base)
                sd s7, 176(\base)
                sd s8, 184(\base)
                sd s9, 192(\base)
                sd s10, 200(\base)
                sd s11, 208(\base)
                sd t3, 216(\base)
                sd t4, 224(\base)
                sd t5, 232(\base)
                sd t6, 240(\base)
# we don't save t6 here, due to we've used it as base, we have to save t6
# in extra step outside of reg_save
        .endm

        .macro restorereges base
                ld ra, 0(\base)
                ld sp, 8(\base)
                ld gp, 16(\base)
                ld tp, 24(\base)
                ld t0, 32(\base)
                ld t1, 40(\base)
                ld t2, 48(\base)
                ld s0, 56(\base)
                ld s1, 64(\base)
                ld a0, 72(\base)
                ld a1, 80(\base)
                ld a2, 88(\base)
                ld a3, 96(\base)
                ld a4, 104(\base)
                ld a5, 112(\base)
                ld a6, 120(\base)
                ld a7, 128(\base)
                ld s2, 136(\base)
                ld s3, 144(\base)
                ld s4, 152(\base)
                ld s5, 160(\base)
                ld s6, 168(\base)
                ld s7, 176(\base)
                ld s8, 184(\base)
                ld s9, 192(\base)
                ld s10, 200(\base)
                ld s11, 208(\base)
                ld t3, 216(\base)
                ld t4, 224(\base)
                ld t5, 232(\base)
                ld t6, 240(\base)
        .endm

###################################

/*
        1 保存进行状态。保存sepc寄存器，储存在中断发生前，pc的值
        2 设置内核栈
        3 跳转到中断处理入口
        4 恢复要运行的进程
*/
.globl kernelvec1
.align 4
kernelvec1:
#1：
# sscratch always saves current process state data, pointing to struct process
        csrrw a0, sscratch, a0  # swap a0 and sscratch, a0 now points to process
        saveregs a0             # save all registers to process context

        # save original a0 value (now in sscratch)
        csrr t5, sscratch       # t5 = original a0 value
        sd t5, 72(a0)           # save original a0 to context.a0 position
        # 必须要恢复sscratch
        # csrw sscratch, t5 # 恢复sscratch

        # restore scratch to process pointer
        csrw sscratch, a0        # sscratch = process pointer
#2
        la sp, stack0
        # li 将常量存放到a0 reg
        li a0, 4096 # 栈从高地址到低地址
        add sp, sp, a0

        csrr a0, sepc # a0作为第一个参数，传递给函数kerneltrap1
        call kerneltrap1

        csrw sepc, a0 # a0是函数kerneltrap1的返回值。spec用于sret恢复pc reg
        csrr t6, sscratch
        restorereges t6 # 恢复所有寄存器
        sret


        #
        # machine-mode timer interrupt.
        #
.globl timervec
.align 4
timervec:
        # start.c has set up the memory that mscratch points to:
        # scratch[0,8,16] : register save area.
        # scratch[24] : address of CLINT's MTIMECMP register.
        # scratch[32] : desired interval between interrupts.
        
        csrrw a0, mscratch, a0
        sd a1, 0(a0)
        sd a2, 8(a0)
        sd a3, 16(a0)

        # schedule the next timer interrupt
        # by adding interval to mtimecmp.
        ld a1, 24(a0) # CLINT_MTIMECMP(hart)
        ld a2, 32(a0) # interval
        ld a3, 0(a1)
        add a3, a3, a2
        sd a3, 0(a1)

        # arrange for a supervisor software interrupt
        # after this handler returns.
        li a1, 2
        csrw sip, a1

        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0

        mret
