#include "defs.h"
#include "proc.h"
#include "spinlock.h"
#include "types.h"
#include "file.h"
#include "pipe.h"
#include "kalloc.h"
#include "vm.h"

#define PIPESIZE 512

struct pipe {
	struct spinlock lock;
	char			data[PIPESIZE];
	uint			nread; // 可读取数量
	uint			nwrite; // 可写入数量
	bool_t			is_readopen; // read fd is still open
	bool_t			is_writeopen; // write fd is still open
};

/*
int	 pipealloc(struct file **f0, struct file **f1);
void pipeclose(struct pipe *pi, bool_t is_writeable);
int	 pipewrite(struct pipe *pi, uint64 addr, int n);
int	 piperead(struct pipe *pi, uint64 addr, int n);
*/

// 创建管道
int pipealloc(struct file **f0, struct file **f1)
{
	struct pipe *pi = NULL;

	*f0 = *f1 = 0;
	if ((*f0 = filealloc()) == NULL || (*f1 = filealloc()) == NULL) {
		goto bad;
	}
	if ((pi = kalloc()) == NULL) {
		goto bad;
	}
	pi->is_readopen	 = TRUE;
	pi->is_writeopen = TRUE;
	pi->nwrite = pi->nread = 0;
	initlock(&pi->lock, "pipe");
	(*f0)->type		   = F_PIPE;
	(*f0)->is_readable = TRUE;
	(*f0)->is_writable = FALSE;
	(*f0)->pipe		   = pi;
	(*f1)->type		   = F_PIPE;
	(*f1)->is_readable = FALSE;
	(*f1)->is_writable = TRUE;
	(*f1)->pipe		   = pi;
	return 0;

bad:
	if (pi) {
		kfree(pi);
	}
	if (*f0) {
		fileclose(*f0);
	}
	if (*f1) {
		fileclose(*f1);
	}
	return -1;
}

void pipeclose(struct pipe *pi, bool_t is_writeable)
{
	acquire(&pi->lock);

	if (is_writeable) {
		pi->is_writeopen = FALSE;
		wakeup(&pi->nread);
	} else {
		pi->is_readopen = FALSE;
		wakeup(&pi->nwrite);
	}

	release(&pi->lock);
	if (pi->is_readopen == FALSE && pi->is_writeopen == FALSE) {
		kfree(pi);
	}
}

int pipewrite(struct pipe *pi, uint64 addr, int n)
{
	int				i  = 0;
	struct process *pr = myproc();

	acquire(&pi->lock);
	while (i < n) {
		if (pi->is_readopen == FALSE || killed(pr)) {
			release(&pi->lock);
			return -1;
		}
		if (pi->nwrite == pi->nread + PIPESIZE) { //DOC: pipewrite-full
			wakeup(&pi->nread);
			sleep(&pi->nwrite, &pi->lock);
		} else {
			char ch;
			if (copyin(pr->pagetable, &ch, addr + i, 1) == -1)
				break;
			pi->data[pi->nwrite++ % PIPESIZE] = ch;
			i++;
		}
	}
	wakeup(&pi->nread);
	release(&pi->lock);

	return i;
}

int piperead(struct pipe *pi, uint64 addr, int n)
{
	int				i  = 0;
	struct process *pr = myproc();
	char			ch;

	acquire(&pi->lock);
	while (pi->nread == pi->nwrite && pi->is_writeopen) { //DOC: pipe-empty
		if (killed(pr)) {
			release(&pi->lock);
			return -1;
		}
		sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
	}
	for (i = 0; i < n; i++) { //DOC: piperead-copy
		if (pi->nread == pi->nwrite)
			break;
		ch = pi->data[pi->nread++ % PIPESIZE];
		if (copyout(pr->pagetable, addr + i, &ch, 1) == -1)
			break;
	}
	wakeup(&pi->nwrite); //DOC: piperead-wakeup
	release(&pi->lock);

	return i;
}
