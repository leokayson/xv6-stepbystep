#ifndef __UART_H__
#define __UART_H__

void uartinit();
void uartstart();
void uartputs(char *s);
void uartputc(char c);
void uartputc_sync(char c);
int	 uartgetc();
void uartintr();
void uartsleep(int sec);

#endif // !__UART_H__
