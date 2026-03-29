#ifndef __FILE_H__
#define __FILE_H__

/*
    file.h的作用：
        1. 内存中的inode对象
        2. 抽象除了file对象，放在process结构体中，并且有多个；
        3. file对象，主要就是包含inode对象和偏移
        4. 可读可写，这个属性由上层判断
*/

#include "types.h"
#include "sleeplock.h"

#define NDIRECT	  12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE	  (NDIRECT + NINDIRECT)

#define major(dev)	((dev) >> 16 & 0xFFFF)
#define minor(dev)	((dev) & 0xFFFF)
#define mkdev(m, n) ((uint)((m) << 16 | (n)))

typedef enum {
	I_NONE	 = 0,
	I_DIR	 = 1, // Directory
	I_FILE	 = 2, // File
	I_DEVICE = 3, // Device
} inodetype;

/*

| 字段            | T\_DIR（目录）               				| T\_FILE（普通文件）   	| T\_DEVICE（设备）   	|
| --------------- | ------------------------------------------- | ------------------------- | --------------- 		|
| **type**        | `T_DIR = 1`              					| `T_FILE = 2`    			| `T_DEVICE = 3`  		|
| **major/minor** | ❌ **未使用**（应为 0）          		   | ❌ **未使用**（应为 0）   | ✅ **主/次设备号**    |
| **size**        | 目录文件字节大小（`dirent` 数组总长度） 	| 文件字节数           		| ❌ **未使用**（通常 0） |
| **addrs\[]**    | 指向包含 `dirent` 结构的数据块     			| 指向文件内容数据块       	| ❌ **未使用**       	|
| **nlink**       | 目录硬链接数（至少 2：自身名字 + "."）  	| 硬链接计数           		| 通常为 1           	|
| **ref**         | 内存引用计数（运行时）              		| 同左              		| 同左              	|

*/

// in-memory copy of an inode
struct inode {
	uint			 dev; // Device number
	uint			 inum; // Inode number
	int				 ref; // Reference count
	struct sleeplock lock; // protects everything below here
	bool			 is_valid; // inode has been read from disk?

	inodetype type; // copy of disk inode
	short	  major;
	short	  minor;
	short	  nlink;
	uint	  size;
	uint	  addrs[NDIRECT + 1];
};

/* ====================== stat.h ====================== */
/*
    1. 主要是描述inode的状态，供上层应用查看；
    2. xb6只实现了3种类型，文件、目录、设备
    3. linux下，一切皆是文件，即一切都可以通过文件接口API访问
*/
struct stat {
	int		  dev; // File system's disk device
	uint	  ino; // Inode number
	inodetype type; // Type of file
	short	  nlink; // Number of links to file
	uint64	  size; // Size of file in bytes
};

#endif // !__FILE_H__