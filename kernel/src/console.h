#ifndef __CONSOLE_H__
#define __CONSOLE_H__
#include "types.h"

void consoleinit();
void consputc(int c);
int	 consoleread(bool_t is_user_dst, uint64 dst, int n);
int	 consolewrite(bool_t is_user_src, uint64 src, int n);
void consoleintr(int c);

#endif // !__CONSOLE_H__