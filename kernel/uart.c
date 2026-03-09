#include "memlayout.h"

// the UART control registers are memory-mapped
// at address UART0. this macro returns the
// address of one of the registers.
#define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// the UART control registers.
// some have different meanings for read vs write.
// see http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

void uartinit()
{
    /*
        1. 关闭中断
        2. 设置波特率为38400 bps
        3. 设置数据位、停止位和校验位 （8位宽度）
        4. 清空输入和输出队列
    */
    WriteReg(IER, 0); // 1. 关闭中断

    /*
        DLL 计算方式：
            波特率 = 时钟频率 / (16 × 除数)
            38400 = 1843200 / (16 × 3)
            38400 = 1843200 / 48
            38400 = 38400  ✓
            
            高字节 (MSB) = 0x00
            低字节 (LSB) = 0x03
        */
    WriteReg(LCR, LCR_BAUD_LATCH); // 2. 设置波特率 - 进入特殊模式
    WriteReg(0, 0x3);   // DLL LSB
    WriteReg(1, 0x0);   // DLL MSB
    WriteReg(LCR, LCR_EIGHT_BITS);

    WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
}

void uartputc(char c) {
    WriteReg(THR, c);
}

void uartputs(char *s) {
    while (s && *s) {
        if (*s == '\n')
            uartputc('\r');
        uartputc(*s++);
    }
}