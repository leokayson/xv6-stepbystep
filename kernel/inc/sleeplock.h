#ifndef __SLEEPLOCK_H__
#define __SLEEPLOCK_H__

#include "types.h"
#include "spinlock.h"

// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?   是否持有锁
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

void initsleeplock(struct sleeplock *lk, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
bool_t is_holdingsleep(struct sleeplock *lk);

#endif // !__SLEEPLOCK_H__
