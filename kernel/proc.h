#ifndef __PROC_H__
#define __PROC_H__

struct cpu {
	int nesting; // 嵌套深度 Depth of push_off() nesting
	int intr_is_enabled; // 中断关闭前的中断状态
};

#endif // !__PROC_H__