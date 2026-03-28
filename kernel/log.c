#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

/*
    文件系统日志层
        1. 支持并发文件系统API
        2. 一次可以提交多个API的更新
        3. 若无活动的文件系统调用，则提交事物
        4. 文件系统APID的调用方法
            begin_op();
            ...
            end_op()
        5. 日志系统：在物理磁盘上的格式为number + block1 + block2
*/

/*
    struct superblock {
        uint magic;        // Must be FSMAGIC
        uint size;         // Size of file system image (blocks)
        uint nblocks;      // Number of data blocks
        uint ninodes;      // Number of inodes.
        uint nlog;         // Number of log blocks
        uint logstart;     // Block number of first log block
        uint inodestart;   // Block number of first inode block
        uint bmapstart;    // Block number of first free map block
    };
*/

struct loghead {
	int n; // 多少个块待写入
	int block[LOGBLOCKS]; // 待写入的快好
};

struct log {
	struct spinlock lock;
	int				start;
	int				size;
	int				outstanding; // 当前文件系统的调用总数
	int				comitting; // 提交标记
	int				dev;
	struct loghead	lh;
} log;

static void install_trans(bool is_recover);
static void read_head();
static void write_head();
static void recover_from_log();
static void write_log();
static void commit();
void		initlog(int dev, struct superblock *sb);
void		log_write(struct buf *b);
void		begin_op();
void		end_op();

void initlog(int dev, struct superblock *sb)
{
	if (sizeof(struct loghead) >= BSIZE) {
		panic("initlog error: too bog loghead");
	}

	initlock(&log.lock, "log");
	log.start = sb->logstart;
	log.size  = sb->nblocks;
	log.dev	  = dev;
	recover_from_log();
}

// 写块号到内存中的日志区，注意此时块中的内容并未写到磁盘日志区
void log_write(struct buf *b)
{
	int i;

	acquire(&log.lock);

	if (log.lh.n >= LOGBLOCKS || log.lh.n >= (log.size - 1)) {
		panic("too big to transact");
	}
	if (log.outstanding < 1) {
		panic("log_write outside of trans");
	}

	for (i = 0; i < log.lh.n; ++i) {
		if (log.lh.block[i] == b->blockno) {
			break;
		}
	}

	// n 动态增加，上面若未匹配到，此时i就是n，可以直接向i放入块号
	log.lh.block[i] = b->blockno;
	// 没有找到情况下，添加新的块到日志去
	if (i == log.lh.n) {
		bpin(b);
		log.lh.n++;
	}
	release(&log.lock);
}

// 文件系统API，开始时调用
// 只自增outstanding
void begin_op()
{
	acquire(&log.lock);
	while (TRUE) {
		if (log.comitting) { // 正在提交
			sleep(&log, &log.lock);
		} else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGBLOCKS) { // 超过预留空间
			sleep(&log, &log.lock);
		} else {
			log.outstanding += 1;
			release(&log.lock);
			break;
		}
	}
}

// 文件系统API，结束时调用
// 此时若未最后一个API，则需要提交事物
void end_op()
{
	// TODO: 为什么这里还需要用do_commit局部呢？有log锁不是log.commit已经可以表示状态了么
	int do_commit = 0;

	acquire(&log.lock);
	log.outstanding -= 1;

	if (log.comitting) { // 提交时有锁，不可能出现这种情况
		panic("log.comitting");
	}

	if (log.outstanding == 0) { // 此时没有文件系统调用，则提交
		do_commit	  = 1;
		log.comitting = 1;
	} else {
		// 其他文件系统调用API将要写入，需要等待可用log区域
		// 此时即便唤醒，也有log.lock锁来保护
		wakeup(&log);
	}
	release(&log.lock);

	// 由于在begin_op中，用了睡眠锁，故而其他核心都会睡眠，不会导致日志区出现问题
	if (do_commit) {
		commit();
		acquire(&log.lock);
		log.comitting = 0;
		wakeup(&log);
		release(&log.lock);
	}
}

// Copy committed blocks from log to their home location
static void install_trans(bool is_recover)
{
	int tail;

	for (tail = 0; tail < log.lh.n; tail++) {
		struct buf *lbuf = bread(log.dev, log.start + tail + 1); // read log block
		struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
		memmove(dbuf->data, lbuf->data, BSIZE); // copy block to dst
		bwrite(dbuf); // write dst to disk
		if (is_recover == FALSE)
			bunpin(dbuf);
		brelse(lbuf);
		brelse(dbuf);
	}
}

// Read the log header from disk into the in-memory log header
static void read_head(void)
{
	struct buf	   *buf = bread(log.dev, log.start);
	struct loghead *lh	= (struct loghead *)(buf->data);
	int				i;

	log.lh.n = lh->n;
	for (i = 0; i < log.lh.n; i++) {
		log.lh.block[i] = lh->block[i];
	}
	brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void write_head(void)
{
	struct buf	   *buf = bread(log.dev, log.start);
	struct loghead *hb	= (struct loghead *)(buf->data);
	int				i;
	hb->n = log.lh.n;
	for (i = 0; i < log.lh.n; i++) {
		hb->block[i] = log.lh.block[i];
	}
	bwrite(buf);
	brelse(buf);
}

static void recover_from_log(void)
{
	read_head();
	install_trans(TRUE); // if committed, copy from log to disk
	log.lh.n = 0;
	write_head(); // clear the log
}

// cache --> log 磁盘上
static void write_log()
{
	int tail;

	for (tail = 0; tail < log.lh.n; tail++) {
		// 读取log区域老的数据内容
		struct buf *to = bread(log.dev, log.start + tail + 1);
		// 读取磁盘上的目前储存的老的数据内容
		struct buf *from = bread(log.dev, log.lh.block[tail]);

		memmove(to->data, from->data, BSIZE);
		bwrite(to);
		brelse(from);
		brelse(to);
	}
}

// 提交事物
static void commit()
{
	if (log.lh.n > 0) {
		write_log(); // cache --> log 磁盘上
		write_head(); //
		//  loghead --> log 磁盘上
		install_trans(FALSE); // log 磁盘 -> 真实磁盘位置
		log.lh.n = 0;
		write_head(); // 因为修改了日志头结构，需要重新写入
	}
}
