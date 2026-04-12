int _main() {
    asm (
        "li a0, 0\n\t"
        "li a7, 1\n\t"  // 1 -> SYS_FORK
        "ecall\n\t"
    );

    volatile int num = 11;

begin:
    num += 1;
    goto begin;
    return 0;
}