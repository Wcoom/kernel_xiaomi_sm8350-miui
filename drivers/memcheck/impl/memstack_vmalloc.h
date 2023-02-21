/*
 * memstack_vmalloc.h
 *
 * Vmalloc stack trace function
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

#ifndef __MEMSTACK_VMALLOC_H
#define __MEMSTACK_VMALLOC_H
#include <linux/sizes.h>
#include <linux/types.h>
#include "memstack_imp.h"

static inline int vmalloc_track_on(char *name)
{
	return 0;
}
static inline int vmalloc_track_off(char *name)
{
	return 0;
}

#ifdef CONFIG_DFX_MEMCHECK_STACK
int vmalloc_stack_open(int type);
int vmalloc_stack_close(void);
size_t vmalloc_stack_read(struct mm_stack_info *buf,
	size_t len, int type);
#else
static inline int vmalloc_stack_open(int type)
{
	return 0;
}
static inline int vmalloc_stack_close(void)
{
	return 0;
}
static inline size_t vmalloc_stack_read(struct mm_stack_info *buf,
	size_t len, int type)
{
	return 0;
}
#endif
#endif /* __MEMSTACK_VMALLOC_H */

