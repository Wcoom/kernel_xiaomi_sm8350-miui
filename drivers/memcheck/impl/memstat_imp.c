/*
 * memstat_imp.c
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

#include "memstat_imp.h"
#include <linux/sizes.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/version.h>
#ifdef CONFIG_CMA
#include <linux/cma.h>
#endif
#include "memdetail_imp.h"
#include "memstack_buddy.h"

#define pages_to_byte(pages) ((pages) << PAGE_SHIFT)

static const char * const track_text[] = {
	"ION_TRACK",
	"SLUB_TRACK",
	"LSLUB_TRACK",
	"VMALLOC_TRACK",
	"CMA_TRACK",
	"ZSPAGE_TRACK",
	"BUDDY_TRACK",
	"SKB_TRACK",
	"ASHMEM_TRACK",
	"GPU_TRACK",
};

static size_t get_stats_cma(void);
static size_t get_stats_slub(void);
static size_t get_stats_lslub(void);
static size_t get_stats_vmalloc(void);
static size_t get_stats_skb(void);
static size_t get_stats_zspage(void);
static size_t get_stats_buddy(void);

struct mem_stat_trace {
	int type;
	size_t (*get_mem_stats)(void);
};

static struct mem_stat_trace memstat_ops[] = {
	{ VMALLOC_TRACK, get_stats_vmalloc },
	{ BUDDY_TRACK, get_stats_buddy },
	{ SLUB_TRACK, get_stats_slub },
	{ LSLUB_TRACK, get_stats_lslub },
	{ SKB_TRACK, get_stats_skb },
	{ ASHMEM_TRACK, get_stats_ashmem },
	{ GPU_TRACK, get_stats_gpu },
	{ ZSPAGE_TRACK, get_stats_zspage },
	{ ION_TRACK, get_stats_ion },
	{ CMA_TRACK, get_stats_cma }
};

static size_t get_stats_cma(void)
{
	return pages_to_byte(totalcma_pages);
}

static size_t get_stats_slub(void)
{
	return pages_to_byte(global_node_page_state(NR_SLAB_UNRECLAIMABLE) +
		global_node_page_state(NR_SLAB_RECLAIMABLE));
}

static size_t get_stats_lslub(void)
{
	return pages_to_byte(global_zone_page_state(NR_LSLAB_PAGES));
}

static size_t get_stats_vmalloc(void)
{
	unsigned int i;
	size_t size = 0;

	for (i = 0; i < ARRAY_SIZE(vmalloc_type); i++)
		size += vm_type_detail_get(vmalloc_type[i]);

	return size;
}

static size_t get_stats_zspage(void)
{
	return pages_to_byte(global_zone_page_state(NR_ZSPAGES));
}

static size_t get_stats_skb(void)
{
	return pages_to_byte(global_zone_page_state(NR_SKB_PAGES));
}

static size_t get_stats_buddy(void)
{
	struct sysinfo i;

	si_meminfo(&i);
	return pages_to_byte(i.totalram - i.freeram);
}

size_t mm_get_mem_total(int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(memstat_ops); i++)
		if (type == memstat_ops[i].type)
			return memstat_ops[i].get_mem_stats();

	return 0;
}

void mm_mem_stats_show(void)
{
	size_t size;
	int i;

	pr_err("========mem stat start==========\n");
	for (i = START_TRACK; i < NR_TRACK; i++) {
		size = mm_get_mem_total(i);
		pr_err("%s used: %ld kB\n", track_text[i], size / SZ_1K);
	}
	pr_err("=========mem stat end==========\n");
}

void mm_set_vmalloc_page_zone_state(struct page *page, bool is_add)
{
	if (!page) {
		pr_err("%s %d error, page is null\n", __func__, __LINE__);
		return;
	}
	if (is_add) {
		SetPageVmalloc(page);
		mod_zone_page_state(page_zone(page), NR_VMALLOC_PAGES, 1);
	} else {
		mod_zone_page_state(page_zone(page), NR_VMALLOC_PAGES, -1);
	}
}

void mm_set_skb_pages_zone_state(struct page *page, unsigned int order,
				 bool is_add)
{
	int i;
	struct page *newpage = NULL;

	if (!page) {
		pr_err("%s page is null\n", __func__);
		return;
	}

	if (!is_add) {
		mod_zone_page_state(page_zone(page), NR_SKB_PAGES, -(1 << order));
		return;
	}

	newpage = page;
	for (i = 0; i < (1 << order); i++) {
		if (!newpage)
			break;
		SetPageSKB(newpage);
		newpage++;
	}
	mod_zone_page_state(page_zone(page), NR_SKB_PAGES, 1 << order);
}

void mm_track_lslub_pages(struct page *page, unsigned int order, unsigned long caller,
			  bool is_add)
{
	if (!page) {
		pr_err("%s page is null\n", __func__);
		return;
	}
	if (is_add) {
		/* set caller */
		mm_set_lslub_track(page, order, caller);
		mod_zone_page_state(page_zone(page), NR_LSLAB_PAGES, 1 << order);
	} else {
		mod_zone_page_state(page_zone(page), NR_LSLAB_PAGES, -(1 << order));
	}
}
