#ifndef __EXEC_H__
#define __EXEC_H__

int exec(char *path, char **argv);
int flags2perm(int flags);

#endif // !__EXEC_H__