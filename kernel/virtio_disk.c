#include "defs.h"
#include "fs.h"
#include "memlayout.h"
#include "types.h"
#include "virtio.h"
#include "riscv.h"
#include "buf.h"

#define Reg(r) (*(volatile uint32 *)(VIRTIO0 + r))

static struct disk {
	struct virtq_desc  *desc;
	struct virtq_avail *avail; // OS -> Physical Device
	struct virtq_used  *used; // Physical Device -> OS

	bool   is_free[NUM];
	uint16 used_idx;

	// 跟踪每个描述符状态信息，比表示读写块成功
	struct {
		struct buf *b;
		uint8		status;
	} info[NUM];

	struct virtio_blk_req ops[NUM];
	struct spinlock vdisk_lock;
} memdisk;

/*
 * static int alloc_desc();
 * static void free_desc(int i);
 * static int alloc3_desc(int *idx);
 * static void free_chain(int i);
 * void virtio_disk_init(void);
 * void virtio_disk_rw(struct buf *b, bool is_write);
 * void  virtio_disk_intr(void);
 * void virtio_disk_test(void);
*/

// 分配一个空描述符
static int alloc_desc()
{
	for (int i = 0; i < NUM; i++) {
		if (memdisk.is_free[i]) {
			memdisk.is_free[i] = FALSE;
			return i;
		}
	}
	return -1;
}

// 释放指定描述符
static void free_desc(int i)
{
	if (i >= NUM || i < 0)
		panic("invlid desc index");
	if (memdisk.is_free[i])
		panic("not free");
	memdisk.desc[i].addr  = 0;
	memdisk.desc[i].len	  = 0;
	memdisk.desc[i].flags = 0;
	memdisk.desc[i].next  = 0;
	memdisk.is_free[i]	  = TRUE;
	// wakeup(&memdisk.is_free[0]);
}

// 分配3个描述符
static int alloc3_desc(int *idx)
{
	for (int i = 0; i < 3; i++) {
		if ((idx[i] = alloc_desc()) < 0) {
			for (int j = 0; j < i; j++) {
				free_desc(idx[j]);
			}
			return -1;
		}
	}
	return 0;
}

// 释放特定描述符链
static void free_chain(int i)
{
	while (TRUE) {
		int flag = memdisk.desc[i].flags;
		int next = memdisk.desc[i].next;

		free_desc(i);

		if (flag & VRING_DESC_F_NEXT) {
			i = next;
		} else {
			break;
		}
	}
}

void virtio_disk_init(void)
{
	/*
		1. 异常排除
		2. 告知设备，已经被发现，并且找到了可用的驱动
		3. 和设备协商一些特性，如果协商失败，那么需要禁止安装驱动
		4. 分配3个数据结构，并告知驱动
		5. 告知设备队列大小
		6. 告知设备协商完毕
	*/

	initlock(&memdisk.vdisk_lock, "virtio_disk");
	
	uint32 status = 0;
	if (Reg(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 || Reg(VIRTIO_MMIO_VERSION) != 2 ||
		Reg(VIRTIO_MMIO_DEVICE_ID) != 2 || Reg(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
		panic("cannot find virtio disk");
	}

	// reset device
	Reg(VIRTIO_MMIO_STATUS) = status;

	// set ACK status bit
	status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
	Reg(VIRTIO_MMIO_STATUS) = status;

	// set Driver status bit
	status |= VIRTIO_CONFIG_S_DRIVER;
	Reg(VIRTIO_MMIO_STATUS) = status;

	// negotiate features
	uint64 features = Reg(VIRTIO_MMIO_DRIVER_FEATURES);
	features &= ~((1 << VIRTIO_BLK_F_RO) | (1 << VIRTIO_BLK_F_SCSI) | (1 << VIRTIO_BLK_F_CONFIG_WCE) |
				  (1 << VIRTIO_BLK_F_MQ) | (1 << VIRTIO_F_ANY_LAYOUT) | (1 << VIRTIO_RING_F_INDIRECT_DESC) |
				  (1 << VIRTIO_RING_F_EVENT_IDX));
	Reg(VIRTIO_MMIO_DRIVER_FEATURES) = features;

	status |= VIRTIO_CONFIG_S_FEATURES_OK;
	Reg(VIRTIO_MMIO_STATUS) = status;

	status = Reg(VIRTIO_MMIO_STATUS);
	if (!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
		panic("Cannot negotiate feature");
	}

	// check maximum queue size
	uint32 max = Reg(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (max == 0) {
		panic("virtio queue num max == 0");
	}
	if (max < NUM) {
		panic("virtio queue num max too short");
	}

	// allocate and zerro queue memory
	memdisk.desc  = kalloc();
	memdisk.avail = kalloc();
	memdisk.used  = kalloc();
	if (!memdisk.desc || !memdisk.avail || !memdisk.used) {
		panic("failed to kaaloc virtio disk");
	}
	memset(memdisk.desc, 0, PGSIZE);
	memset(memdisk.avail, 0, PGSIZE);
	memset(memdisk.used, 0, PGSIZE);

	// set queue size
	Reg(VIRTIO_MMIO_QUEUE_NUM) = NUM;

	// write physical memory addresses
	Reg(VIRTIO_MMIO_QUEUE_DESC_LOW)	  = (uint64)memdisk.desc;
	Reg(VIRTIO_MMIO_QUEUE_DESC_HIGH)  = (uint64)memdisk.desc >> 32;
	Reg(VIRTIO_MMIO_DRIVER_DESC_LOW)  = (uint64)memdisk.avail;
	Reg(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)memdisk.avail >> 32;
	Reg(VIRTIO_MMIO_DEVICE_DESC_LOW)  = (uint64)memdisk.used;
	Reg(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)memdisk.used >> 32;

	// queue is ready
	Reg(VIRTIO_MMIO_QUEUE_READY) = 1;

	// all NUM desc starat out unused
	for (int i = 0; i < NUM; ++i) {
		memdisk.is_free[i] = 1;
	}

	// tell device we're completely ready
	status |= VIRTIO_CONFIG_S_DRIVER_OK;
	Reg(VIRTIO_MMIO_STATUS) = status;

	// plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// 读写磁盘块
void virtio_disk_rw(struct buf *b, bool is_write)
{
	uint64 sector = b->blockno * (BSIZE / 512);

	// 3 desc: type/reserved/sector; data; 1B status result
	int idx[3];
	while (1) {
		if (alloc3_desc(idx) == 0) {
			break;
		}
		// TODO 请求不成功，睡眠当前线程等待
	}

	// 格式化3个描述符
	struct virtio_blk_req *buf0 = &memdisk.ops[idx[0]];
	if (is_write) {
		buf0->type = VIRTIO_BLK_T_OUT;
	} else {
		buf0->type = VIRTIO_BLK_T_IN;
	}
	buf0->reserved = 0;
	buf0->sector   = sector;

	memdisk.desc[idx[0]].addr  = (uint64)buf0;
	memdisk.desc[idx[0]].len   = sizeof(struct virtio_blk_req);
	memdisk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
	memdisk.desc[idx[0]].next  = idx[1];

	memdisk.desc[idx[1]].addr  = (uint64)b->data;
	memdisk.desc[idx[1]].len   = BSIZE;
	memdisk.desc[idx[1]].flags = VRING_DESC_F_NEXT | (is_write ? 0 : VRING_DESC_F_WRITE); // devices reads b->data or write b->data
	memdisk.desc[idx[1]].next  = idx[2];

	memdisk.info[idx[0]].status = 0xFF; // device writes 0 on success

	memdisk.desc[idx[2]].addr  = (uint64)&memdisk.info[idx[0]].status;
	memdisk.desc[idx[2]].len   = 1;
	memdisk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // devices or the status
	memdisk.desc[idx[2]].next  = 0;

	// record struct buf for virtio_memdisk_intr()
	b->disk				   = 1; // 在中断中将此值置为0，表示处理完毕
	memdisk.info[idx[0]].b = b;

	// tell the device the first index in our chain of descriptors
	memdisk.avail->ring[memdisk.avail->idx % NUM] = idx[0];

	__sync_synchronize();

	// tell the device another avail ring entry is avilable
	memdisk.avail->idx += 1;

	__sync_synchronize();

	Reg(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

	// printf("VA1");
	// Wait for virtio_memdisk_intr() to say request has finished
	while (b->disk == 1) {
		// TODO 增加睡眠锁等待
	}
	// printf("VA2");

	memdisk.info[idx[0]].b = 0;
	free_chain(idx[0]);
}

void virtio_disk_intr(void)
{
	// TODO 增加锁

	// 告知虚拟磁盘设备我们可以响应下一个中断（一次可以完成多个）
	Reg(VIRTIO_MMIO_INTERRUPT_ACK) = Reg(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

	// 当设备处理完一个请求时，设备自动将memdisk.used->idx+1
	while (memdisk.used_idx != memdisk.used->idx) {
		__sync_synchronize();
		uint16 id = memdisk.used->ring[memdisk.used_idx % NUM].id;

		if (memdisk.info[id].status != 0) {
			panic("virtio_disk_intr status");
		}

		struct buf *b = memdisk.info[id].b;
		b->disk		  = 0; // disk is done with buf

		// TODO 唤醒
		memdisk.used_idx += 1;
	}
}

void virtio_disk_test(void)
{
	struct buf b_test[3];

	for (int i = 0; i < 3; i++) {
		b_test[i].dev = 1;
		b_test[i].blockno = i + 1; // 写入的磁盘号
		for (int j = 0; j < BSIZE; ++j) {
			b_test[i].data[j] = 0x33 + i;
		}
	}
	
	printf("b_test write start...\n");
	virtio_disk_rw(&b_test[0], TRUE);
	virtio_disk_rw(&b_test[1], TRUE);
	virtio_disk_rw(&b_test[2], TRUE);
	printf("b_test write end...\n");
}
