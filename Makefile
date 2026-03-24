# 伪目标声明
.PHONY: build clean qemu gdb rebuild

# 目录定义
K := kernel

# 工具链前缀
TOOLPREFIX := riscv64-unknown-elf-
CC := $(TOOLPREFIX)gcc
# AS := $(TOOLPREFIX)gas # 通常不需要单独调用 gas，gcc 会处理
# LD := $(TOOLPREFIX)ld  # 链接时建议直接用 CC (gcc)，而不是直接用 ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

# --- 核心修改：定义 OBJS 变量 ---
OBJS := \
	$(K)/entry.o \
	$(K)/kernelvec.o \
	$(K)/start.o \
	$(K)/uart.o \
	$(K)/main.o \
	$(K)/trap.o \
	$(K)/kalloc.o \
	$(K)/string.o \
	$(K)/printf.o \
	$(K)/vm.o \
	$(K)/proc.o \
	$(K)/spinlock.o \
	$(K)/sleeplock.o \
	$(K)/plic.o \
	$(K)/virtio_disk.o \
	$(K)/swtch.o \
	$(K)/trampoline.o \
	$(K)/syscall.o \

# 编译标志
CFLAGS := -Wall -Werror -O0 \
	-fno-omit-frame-pointer -ggdb \
	-MD -mcmodel=medany \
	-ffreestanding -nostdlib -fno-common \
	-fno-stack-protector -fno-pie

# 链接标志
# 注意：这里保留 -Wl, 是因为我们将使用 CC (gcc) 来执行链接命令
LDFLAGS := -static -nostdlib -Wl,--no-relax -z max-page-size=4096

# 默认目标
build: $(K)/kernel.elf

# 链接规则：使用 $(CC) 而不是 $(LD)
# gcc 会自动处理 -Wl, 前缀并将其传递给底层的 ld
$(K)/kernel.elf: $(OBJS) $(K)/kernel.ld
	$(CC) $(LDFLAGS) -T $(K)/kernel.ld -o $@ $(OBJS)
	$(OBJDUMP) -S $(K)/kernel.elf > $K/kernel.asm
	$(OBJDUMP) -d $(K)/kernel.elf > $K/kernel.elf.objdump.txt
	$(OBJDUMP) -t $(K)/kernel.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(K)/kernel.sym

# 编译规则：C 文件
$(K)/%.o: $(K)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 编译规则：汇编文件
$(K)/%.o: $(K)/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# CPU 数
CPU := 1

# QEMU 配置
QEMU := qemu-system-riscv64
QEMU_FLAGS := -machine virt -nographic -bios none -kernel $(K)/kernel.elf -m 128 -smp $(CPU)
QEMU_FLAGS += -global virtio-mmio.force-legacy=false
QEMU_FLAGS += -drive file=fs.img,if=none,format=raw,id=x0
QEMU_FLAGS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

fs.img:
	dd if=/dev/urandom of=fs.img bs=1M count=6

qemu: build fs.img
	$(QEMU) $(QEMU_FLAGS)

gdb: build fs.img
	$(QEMU) $(QEMU_FLAGS) -S -s & \
	gdb -q -x gdbinit

clean:
	rm -f $(K)/*.o $(K)/*.d $(K)/kernel.elf $(K)/*.asm $(K)/*.sym fs.img

rebuild: clean build

# 包含自动生成的依赖文件
-include $(OBJS:.o=.d)
