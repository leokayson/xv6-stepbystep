#ifndef __BIO_H__
#define __BIO_H__

#include "types.h"

void binit();
struct buf *bread(uint dev, uint blocko);
void bwrite(struct buf *b);
void bpin(struct buf *b);
void bunpin(struct buf *b);
void brelse(struct buf *b);

#endif // !__BIO_H__
