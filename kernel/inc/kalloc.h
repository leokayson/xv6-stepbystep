#ifndef __KALLOC_H__
#define __KALLOC_H__

void *kalloc(void);
void  kfree(void *);
void  kinit(void);
void  kmemtest();

#endif // !__KALLOC_H__
