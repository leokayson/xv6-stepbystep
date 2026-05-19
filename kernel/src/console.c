#include "console.h"
#include "file.h"
#include "proc.h"
#include "spinlock.h"
#include "types.h"
#include "uart.h"

#define BACKSPACE	   0x100
#define C(x)		   ((x) - '@') // Control-x
#define INPUT_BUF_SIZE 128

struct {
	struct spinlock lock;

	// input
	char buf[INPUT_BUF_SIZE];
	uint r; // Read index
	uint w; // Write index
	uint e; // Edit index
} cons;

void consputc(int c)
{
	if (c == BACKSPACE) {
		uartputc_sync('\b');
		uartputc_sync(' ');
		uartputc_sync('\b');
	} else {
		uartputc_sync(c);
	}
}

int consoleread(bool_t is_user_dst, uint64 dst, int n)
{
	uint target;
	int	 c;
	char cbuf;

	target = n;
	acquire(&cons.lock);

	while (n > 0) {
		while (cons.r == cons.w) {
			if (killed(myproc())) {
				release(&cons.lock);
				return -1;
			}
			sleep(&cons.r, &cons.lock);
		}

		c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

		if (c == C('D')) { // end-of-file
			if (n < target) {
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				cons.r--;
			}
			break;
		}

		// copy the input byte to the user-space buffer.
		cbuf = c;
		if (either_copyout(is_user_dst, dst, &cbuf, 1) == -1)
			break;

		dst++;
		--n;

		if (c == '\n') {
			// a whole line has arrived, return to
			// the user-level read().
			break;
		}
	}
	release(&cons.lock);

	return target - n;
}

int consolewrite(bool_t is_user_src, uint64 src, int n)
{
	int i;

	for (i = 0; i < n; ++i) {
		char c;
		if (either_copyin(is_user_src, &c, src + i, 1) == -1)
			break;
		uartputc(c);
	}

	return i;
}

void consoleintr(int c)
{
	acquire(&cons.lock);

	switch (c) {
		case C('P'): { // ctrl + p 打印当前进程列表
			procdump();
			break;
		}
		case C('U'): { // ctrl + u 清空当前行
			while (cons.e != cons.w && cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
				cons.e--;
				consputc(BACKSPACE);
			}
			break;
		}
		case C('H'): // ctrl + h / backsapce / delete key
		case '\x7f': {
			if (cons.e != cons.w) {
				cons.e--;
				consputc(BACKSPACE);
			}
			break;
		}
		default: {
			if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
				c = (c == '\r') ? '\n' : c;
				consputc(c); // echo c 给用户显示
				cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
				if (c == '\n' || c == C('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
					cons.w = cons.e;
					wakeup(&cons.r);
				}
			}
			break;
		}
	}

	release(&cons.lock);
}

void consoleinit()
{
    extern struct devsw devsw[NDEV];

	initlock(&cons.lock, "cons");
	uartinit();

	devsw[CONSOLE].read	 = consoleread;
	devsw[CONSOLE].write = consolewrite;
}