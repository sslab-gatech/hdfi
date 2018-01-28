#ifndef _ASM_RISCV_CURRENT_H
#define _ASM_RISCV_CURRENT_H

#include <asm/csr.h>

struct task_struct;

register unsigned long current_thread_pointer asm ("tp");

static inline struct task_struct *get_current(void)
{
	return (struct task_struct*)current_thread_pointer;
}

#define current (get_current())

#endif /* _ASM_RISCV_CURRENT_H */
