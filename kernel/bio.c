#include "defs.h"
#include "types.h"
#include "buf.h"
#include "param.h"
#include "spinlock.h"

/*
    bcache: 缓存常用块，内存中以双链表形式存放
    对外接口：
        获取缓存bread -> bwrite -> brelse
*/
struct {
	struct spinlock lock;
	struct buf		buf[NBUF];

	// Linked list of all buffers, through prev/next.
	// Sorted by how recently the buffer was used.
	// head.next is most recent, head.prev is least.
	struct buf head;
} bcache;

void binit()
{
	struct buf *b;

	initlock(&bcache.lock, "bcache");

	// Create linked list of buffers
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;
	for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		initsleeplock(&b->lock, "buffer");
		bcache.head.next->prev = b;
		bcache.head.next	   = b;
	}
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno)
{
	struct buf *b;

	acquire(&bcache.lock);

	// Is the block already cached?
    // 在链表中，直接返回locked buf
	for (b = bcache.head.next; b != &bcache.head; b = b->next) {
		if (b->dev == dev && b->blockno == blockno) {
			b->refcnt++;
			release(&bcache.lock);
			acquiresleep(&b->lock);
			return b;
		}
	}

	// Not cached.
	// Recycle the least recently used (LRU) unused buffer.
	for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
		if (b->refcnt == 0) {
			b->dev	   = dev;
			b->blockno = blockno;
			b->valid   = 0;  // 表明当前块还未被从磁盘中读取
			b->refcnt  = 1;
			release(&bcache.lock);
			acquiresleep(&b->lock);
			return b;
		}
	}
	panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno)
{
	struct buf *b;

	b = bget(dev, blockno);
	if (!b->valid) {
		virtio_disk_rw(b, 0); // 从磁盘中读取
		b->valid = 1;
	}
	return b;
}

// Write b's contents to disk. Must be locked.
// 写入到物理磁盘
void bwrite(struct buf *b)
{
	if (!is_holdingsleep(&b->lock))
		panic("bwrite");
	virtio_disk_rw(b, 1);
}

// 固定在内存
void bpin(struct buf *b)
{
	acquire(&bcache.lock);
	b->refcnt++;
	release(&bcache.lock);
}

// 释放内存中的块
void bunpin(struct buf *b)
{
	acquire(&bcache.lock);
	b->refcnt--;
	release(&bcache.lock);
}

// 释放块
void brelse(struct buf *b)
{
	// 减少引用。若引用为0,将块放入经常使用的链表中
	if (!is_holdingsleep(&b->lock)) {
		panic("brelse");
	}
	releasesleep(&b->lock);

	acquire(&bcache.lock);
	if (--b->refcnt == 0) {
		// 此块不再被使用。可以释放，即将此块移动到最常使用链表的头部

		// 链表中移除b
		b->next->prev = b->prev;
		b->prev->next = b->next;

		// 将b放到bcache.head的next处
		b->next				   = bcache.head.next;
		b->prev				   = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next	   = b;
	}
	release(&bcache.lock);
}
