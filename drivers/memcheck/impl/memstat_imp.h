/*
 * memstat_imp.h
 *
 * Get memory statistic data function
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

#ifndef __MEMSTAT_IMP_H
#define __MEMSTAT_IMP_H
#include <linux/sizes.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include "memtrace_ion.h"
#include "memtrace_ashmem.h"
#include "memtrace_gpumem.h"

#define VM_LAZY_FREE	0x02
#define VM_VM_AREA	0x04
#define VM_VALID_FLAG (VM_ALLOC | VM_IOREMAP | VM_MAP | VM_USERMAP)

enum {
	START_TRACK,
	ION_TRACK = START_TRACK,
	SLUB_TRACK,
	LSLUB_TRACK,
	VMALLOC_TRACK,
	CMA_TRACK,
	ZSPAGE_TRACK,
	BUDDY_TRACK,
	SKB_TRACK,
	ASHMEM_TRACK,
	GPU_TRACK,
	NR_TRACK,
};
enum {
	SLUB_ALLOC,
	SLUB_FREE,
	NR_SLUB_ID,
};

static const int vmalloc_type[] = { VM_IOREMAP, VM_ALLOC, VM_MAP, VM_USERMAP };
static const char * const vmalloc_text[] = {
	"VM_IOREMAP",
	"VM_ALLOC",
	"VM_MAP",
	"VM_USERMAP",
};

#ifdef CONFIG_DFX_MEMCHECK
size_t mm_get_mem_total(int type);
void mm_set_skb_pages_zone_state(struct page *page, unsigned int order, bool is_add);
void mm_track_lslub_pages(struct page *page, unsigned int order, unsigned long caller, bool is_add);
#else
static inline size_t mm_get_mem_total(int type)
{
	return 0;
}
static inline void mm_set_skb_pages_zone_state(struct page *page, unsigned int order, bool is_add)
{
}
static inline void mm_track_lslub_pages(struct page *page, unsigned int order, unsigned long caller, bool is_add)
{
}
#endif
#endif /* __MEMSTAT_IMP_H */

