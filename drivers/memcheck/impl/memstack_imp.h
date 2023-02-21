/*
 * memstack_imp.h
 *
 * Stack trace implementation
 *
 * Copyright(C) 2021 Huawei Technologies Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MEMSTACK_IMP_H
#define __MEMSTACK_IMP_H
#include <linux/sizes.h>
#include <linux/types.h>

struct mm_stack_info {
	unsigned long caller;
	atomic_t ref;
};

#ifdef CONFIG_DFX_MEMCHECK_STACK
int mm_page_trace_on(int type, char *name);
int mm_page_trace_off(int type, char *name);
int mm_page_trace_open(int type, int subtype);
int mm_page_trace_close(int type, int subtype);
size_t mm_page_trace_read(int type,
	struct mm_stack_info *info, size_t len, int subtype);
#else /* CONFIG_DFX_MEMCHECK_STACK */
static inline int mm_page_trace_on(int type, char *name)
{
	return 0;
}

static inline int mm_page_trace_off(int type, char *name)
{
	return 0;
}

static inline int mm_page_trace_open(int type, int subtype)
{
	return 0;
}

static inline int mm_page_trace_close(int type, int subtype)
{
	return 0;
}

static inline size_t mm_page_trace_read(int type,
	struct mm_stack_info *info, size_t len, int subtype)
{
	return 0;
}
#endif  /* CONFIG_DFX_MEMCHECK_STACK */
#endif /* __MEMSTACK_IMP_H */
