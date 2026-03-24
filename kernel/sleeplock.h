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

#endif // !__SLEEPLOCK_H__