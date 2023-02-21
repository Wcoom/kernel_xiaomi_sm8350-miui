/*
 * memstack_buddy.h
 *
 * Buddy stack trace function
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

#ifndef __MEMSTACK_BUDDY_H
#define __MEMSTACK_BUDDY_H
#include <linux/sizes.h>
#include <linux/types.h>
#include <linux/mm_types.h>
#include "memstack_imp.h"

static inline int buddy_track_on(char *name)
{
	return 0;
}
static inline int buddy_track_off(char *name)
{
	return 0;
}

#ifdef CONFIG_DFX_MEMCHECK_STACK
int buddy_stack_open(int type);
int buddy_stack_close(void);
size_t buddy_stack_read(struct mm_stack_info *buf, size_t len, int type);
void mm_set_buddy_track(struct page *page,
	unsigned int order, unsigned long caller);
void mm_set_lslub_track(struct page *page,
	unsigned int order, unsigned long caller);
int mm_buddy_track_unmap(void);
#else /* CONFIG_DFX_MEMCHECK_STACK */
static inline int buddy_stack_open(int type)
{
	return 0;
}
static inline int buddy_stack_close(void)
{
	return 0;
}
static inline size_t buddy_stack_read(struct mm_stack_info *buf,
	size_t len, int type)
{
	return 0;
}
static inline void mm_set_buddy_track(struct page *page,
	unsigned int order, unsigned long caller)
{
}
static inline void mm_set_lslub_track(struct page *page,
	unsigned int order, unsigned long caller)
{
}
static inline int mm_buddy_track_unmap(void)
{
	return 0;
}
#endif /* CONFIG_DFX_MEMCHECK_STACK */
#endif /* __MEMSTACK_BUDDY_H */

