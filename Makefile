.PHONY: kernel clean qemu gdb

CFLAGS = -Wall -Werror -O0 \
	-fno-omit-frame-pointer -ggdb \
	-MD -mcmodel=medany \
	-ffreestanding -nostdlib -fno-common \
	-fno-stack-protector -fno-pie

LDFLAGS = -static -nostdlib -Wl,--no-relax -z max-page-size=4096

kernel:
	riscv64-unknown-elf-gcc $(CFLAGS) -c kernel/entry.S -o kernel/entry.o
	riscv64-unknown-elf-gcc $(CFLAGS) -c kernel/start.c -o kernel/start.o
	riscv64-unknown-elf-gcc $(LDFLAGS) \
		kernel/entry.o kernel/start.o \
		-T kernel/kernel.ld \
		-o kernel/kernel.elf

QEMU = qemu-system-riscv64 \
	-machine virt \
	-nographic \
	-bios none \
	-kernel kernel/kernel.elf \
	-m 128 \
	-smp 1

qemu: kernel
	$(QEMU)

gdb: kernel
	$(QEMU) -S -s & \
	gdb -q -x gdbinit

clean:
	rm -rf kernel/kernel.elf kernel/*.o kernel/*.d