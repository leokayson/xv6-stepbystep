#include "sleeplock.h"
#include "defs.h"

void initsleeplock(struct sleeplock *lk, char *name) {
    initlock(&lk->lk, name);    
    lk->name = name;
    lk->locked = 0;
    lk->pid = 0;
}

void acquiresleep(struct sleeplock *lk) {
    acquire(&lk->lk);
    while(lk->locked) {
        sleep(lk, &lk->lk);
    }
    lk->locked = 1;
    lk->pid = myproc()->pid;
    release(&lk->lk);
}

void releasesleep(struct sleeplock *lk) {
    acquire(&lk->lk);
    lk->locked = 0;
    lk->pid = 0;
    wakeup(lk);
    release(&lk->lk);
}

bool is_holdingsleep(struct sleeplock *lk) {
    bool r;
    acquire(&lk->lk);
    r = lk->locked && (lk->pid == myproc()->pid);
    release(&lk->lk);
    return r;
}
