#ifndef __SYSFILE_H__
#define __SYSFILE_H__

#include "types.h"

uint64 sys_dup();
uint64 sys_read();
uint64 sys_write();
uint64 sys_close();
uint64 sys_fstat();
uint64 sys_link();
uint64 sys_mkdir();
uint64 sys_mknod();
uint64 sys_chdir();
uint64 sys_exec();
uint64 sys_pipe();
uint64 sys_open();
uint64 sys_unlink();

#endif // !__SYSFILE_H__