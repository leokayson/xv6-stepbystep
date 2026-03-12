.PHONY: kernel clean qemu gdb

K=kernel
U=user

TOOLPREFIX = riscv64-unknown-elf-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# OBJS = \
#   $K/entry.o \
#   $K/start.o \
#   $K/uart.o \

CFLAGS = -Wall -Werror -O0 \
	-fno-omit-frame-pointer -ggdb \
	-MD -mcmodel=medany \
	-ffreestanding -nostdlib -fno-common \
	-fno-stack-protector -fno-pie 

LDFLAGS = -static -nostdlib -Wl,--no-relax -z max-page-size=4096

kernel:
	$(CC) $(CFLAGS) -c kernel/entry.S -o kernel/entry.o
	$(CC) $(CFLAGS) -c kernel/start.c -o kernel/start.o
	$(CC) $(CFLAGS) -c kernel/uart.c -o kernel/uart.o
	$(CC) $(CFLAGS) -c kernel/main.c -o kernel/main.o
	$(CC) $(LDFLAGS) \
		kernel/entry.o kernel/start.o kernel/uart.o \
		kernel/main.o \
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
