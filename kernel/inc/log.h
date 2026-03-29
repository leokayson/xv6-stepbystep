#ifndef __LOG_H__
#define __LOG_H__

#include "virtio.h"
#include "fs.h"

void initlog(int dev, struct superblock *sb);
void log_write(struct buf *b);
void begin_op();
void end_op();

#endif // !__LOG_H__
