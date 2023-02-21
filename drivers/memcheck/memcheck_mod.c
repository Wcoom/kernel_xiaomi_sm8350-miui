/*
 * memcheck_mod.c
 *
 * memory leak detect
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/version.h>
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
#include <platform/trace/hooks/memcheck.h>
#else
#include <platform/trace/events/memcheck.h>
#endif
#include <platform/linux/memcheck.h>

#include "impl/memstack_buddy.h"
#include "impl/memstack_slub.h"
#include "impl/memstat_imp.h"
#include "memcheck_memstat.h"

static void hook_mm_set_buddy_track(void *ignore, struct page *page,
				    unsigned int order, unsigned long caller)
{
	mm_set_buddy_track(page, order, caller);
}
static void hook_mm_track_lslub_pages(void *ignore, struct page *page,
				      unsigned int order, unsigned long caller,
				      bool is_add)
{
	mm_track_lslub_pages(page, order, caller, is_add);
}

static void hook_mm_set_slub_alloc_track(void *ignore, unsigned long caller)
{
	mm_set_slub_alloc_track(caller);
}

static void hook_mm_set_slub_free_track(void *ignore, unsigned long caller)
{
	mm_set_slub_free_track(caller);
}

static void hook_mm_mem_stats_show(void *ignore, int unused)
{
	mm_mem_stats_show();
}

static void hook_mm_vmalloc_detail_show(void *ignore, int unused)
{
	mm_vmalloc_detail_show();
}

static void hook_mm_set_skb_pages_zone_state(void *ignore, struct page *page,
					     unsigned int order, bool is_add)
{
	mm_set_skb_pages_zone_state(page, order, is_add);
}

static int __init dfx_memcheck_init(void)
{
	memcheck_createfs();
	register_trace_mm_set_buddy_track(hook_mm_set_buddy_track, NULL);
	register_trace_mm_track_lslub_pages(hook_mm_track_lslub_pages, NULL);
	register_trace_mm_set_slub_alloc_track(hook_mm_set_slub_alloc_track, NULL);
	register_trace_mm_set_slub_free_track(hook_mm_set_slub_free_track, NULL);
	register_trace_mm_mem_stats_show(hook_mm_mem_stats_show, NULL);
	register_trace_mm_vmalloc_detail_show(hook_mm_vmalloc_detail_show, NULL);
	register_trace_mm_set_skb_pages_zone_state(hook_mm_set_skb_pages_zone_state, NULL);

	return 0;
}
module_init(dfx_memcheck_init);

static void __exit dfx_memcheck_exit(void)
{
	unregister_trace_mm_set_buddy_track(hook_mm_set_buddy_track, NULL);
	unregister_trace_mm_track_lslub_pages(hook_mm_track_lslub_pages, NULL);
	unregister_trace_mm_set_slub_alloc_track(hook_mm_set_slub_alloc_track, NULL);
	unregister_trace_mm_set_slub_free_track(hook_mm_set_slub_free_track, NULL);
	unregister_trace_mm_set_skb_pages_zone_state(hook_mm_set_skb_pages_zone_state, NULL);
}
module_exit(dfx_memcheck_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
MODULE_DESCRIPTION("DFX Memcheck Module");
