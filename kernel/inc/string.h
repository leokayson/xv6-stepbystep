#ifndef __STRING_H__
#define __STRING_H__

int	  memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int	  strlen(const char *);
int	  strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

#endif // !__STRING_H__

