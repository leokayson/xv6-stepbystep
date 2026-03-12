
void main() {
    extern void uartinit();
    extern void uartputs(char *s);

    uartinit();
    uartputs("Hello World\r\n");
}
