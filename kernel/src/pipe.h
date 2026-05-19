#ifndef __PIPE_H__
#define __PIPE_H__
#include "file.h"
#include "types.h"

struct pipe;

int	 pipealloc(struct file **f0, struct file **f1);
void pipeclose(struct pipe *pi, bool_t is_writeable);
int	 pipewrite(struct pipe *pi, uint64 addr, int n);
int	 piperead(struct pipe *pi, uint64 addr, int n);

#endif // !__PIPE_H__