/*
 * memstack_slub.h
 *
 * Slub stack trace function
 *
 * Copyright(C) 2020 Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef __MEMSTACK_SLUB_H
#define __MEMSTACK_SLUB_H
#include <linux/sizes.h>
#include <linux/types.h>
#include "memstack_imp.h"

static inline int slub_stack_open(int type)
{
	return 0;
}
static inline int slub_stack_close(void)
{
	return 0;
}

#ifdef CONFIG_DFX_MEMCHECK_STACK
int slub_track_on(char *name);
int slub_track_off(char *name);
size_t slub_stack_read(struct mm_stack_info *buf,
	size_t len, int type);
void mm_set_slub_alloc_track(unsigned long caller);
void mm_set_slub_free_track(unsigned long caller);
#else
static inline int slub_track_on(char *name)
{
	return 0;
}
static inline int slub_track_off(char *name)
{
	return 0;
}
static inline size_t slub_stack_read(struct mm_stack_info *buf,
	size_t len, int type)
{
	return 0;
}
static inline void mm_set_slub_alloc_track(unsigned long caller)
{
}
static inline void mm_set_slub_free_track(unsigned long caller)
{
}
#endif
#endif /* __MEMSTACK_SLUB_H */

