#ifndef __PRINT_H__
#define __PRINT_H__

int	 printf(char *, ...) __attribute__((format(printf, 1, 2)));
void panic(char *) __attribute__((noreturn));
void printfinit(void);

#endif // !__PRINT_H__
