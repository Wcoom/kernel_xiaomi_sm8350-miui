/*
 * memstack_imp.c
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

#include "memstack_imp.h"
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "memstat_imp.h"
#include "memstack_vmalloc.h"
#include "memstack_buddy.h"
#include "memstack_slub.h"

struct page_stack_trace {
	int (*page_stack_on)(char *name);
	int (*page_stack_off)(char *name);
	int (*page_stack_open)(int type);
	int (*page_stack_close)(void);
	size_t (*page_stack_read)(struct mm_stack_info *buf,
				  size_t len, int type);
};

/* on->off->open->read->close */
static struct page_stack_trace vmalloc_page_stack = {
	.page_stack_on = vmalloc_track_on,
	.page_stack_off = vmalloc_track_off,
	.page_stack_open = vmalloc_stack_open,
	.page_stack_close = vmalloc_stack_close,
	.page_stack_read = vmalloc_stack_read,
};

static struct page_stack_trace slub_page_stack = {
	.page_stack_on = slub_track_on,
	.page_stack_off = slub_track_off,
	.page_stack_open = slub_stack_open,
	.page_stack_close = slub_stack_close,
	.page_stack_read = slub_stack_read,
};

static struct page_stack_trace buddy_page_stack = {
	.page_stack_on = buddy_track_on,
	.page_stack_off = buddy_track_off,
	.page_stack_open = buddy_stack_open,
	.page_stack_close = buddy_stack_close,
	.page_stack_read = buddy_stack_read,
};

struct memstack_trace {
	int type;
	struct page_stack_trace *stack_trace;
};

static struct memstack_trace memstack_ops[] = {
	{ VMALLOC_TRACK, &vmalloc_page_stack },
	{ BUDDY_TRACK, &buddy_page_stack },
	{ SLUB_TRACK, &slub_page_stack },
	{ LSLUB_TRACK, &buddy_page_stack }, /* the same with buddy */
	{ SKB_TRACK, NULL },
	{ ASHMEM_TRACK, NULL },
	{ GPU_TRACK, NULL },
	{ ZSPAGE_TRACK, NULL },
	{ ION_TRACK, NULL },
	{ CMA_TRACK, NULL },
};

int mm_page_trace_on(int type, char *name)
{
	unsigned int i;

	if (!name)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(memstack_ops); i++)
		if (type == memstack_ops[i].type && memstack_ops[i].stack_trace)
			return memstack_ops[i].stack_trace->page_stack_on(name);
	return -EINVAL;
}

int mm_page_trace_off(int type, char *name)
{
	unsigned int i;

	if (!name)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(memstack_ops); i++)
		if (type == memstack_ops[i].type && memstack_ops[i].stack_trace)
			return memstack_ops[i].stack_trace->page_stack_off(name);

	return -EINVAL;
}

int mm_page_trace_open(int type, int subtype)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(memstack_ops); i++)
		if (type == memstack_ops[i].type && memstack_ops[i].stack_trace)
			return memstack_ops[i].stack_trace->page_stack_open(subtype);

	return -EINVAL;
}

int mm_page_trace_close(int type, int subtype)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(memstack_ops); i++)
		if (type == memstack_ops[i].type && memstack_ops[i].stack_trace)
			return memstack_ops[i].stack_trace->page_stack_close();

	return -EINVAL;
}

size_t mm_page_trace_read(int type, struct mm_stack_info *info, size_t len,
	int subtype)
{
	unsigned int i;

	if (!info)
		return 0;
	for (i = 0; i < ARRAY_SIZE(memstack_ops); i++)
		if (type == memstack_ops[i].type && memstack_ops[i].stack_trace)
			return memstack_ops[i].stack_trace->page_stack_read(
				info, len, subtype);

	return 0;
}
