
#define NCPU 8

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

void start() {
    extern void uartinit();
    extern void uartputs(char *s);

    uartinit();
    uartputs("Hello, xv6!\n");
    while(1) {};
}
