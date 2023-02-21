/*
 * memdetail_imp.c
 *
 * Get detailed memory info function
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

#include "memdetail_imp.h"
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/version.h>
#include "slab.h"
#include "memstat_imp.h"
#include "memtrace_ion.h"
#include "memtrace_gpumem.h"
#include "memtrace_ashmem.h"

struct mem_detail_trace {
	int type;
	size_t (*get_mem_detail)(void *buf, size_t len, void *buf_extend);
};

static struct mem_detail_trace memdetail_ops[] = {
	{ VMALLOC_TRACK, get_vmalloc_detail },
	{ BUDDY_TRACK, NULL },
	{ SLUB_TRACK, get_slub_detail },
	{ LSLUB_TRACK, NULL },
	{ SKB_TRACK, NULL },
	{ ASHMEM_TRACK, get_ashmem_detail },
	{ GPU_TRACK, NULL },
	{ ZSPAGE_TRACK, NULL },
	{ ION_TRACK, get_ion_detail },
	{ CMA_TRACK, NULL }
};

/* get vmalloc mem detail info */
size_t vm_type_detail_get(int subtype)
{
	struct vmap_area *va = NULL;
	struct vm_struct *vm = NULL;
	struct list_head *vmap_list = NULL;
	size_t len = 0;
	unsigned long type = (unsigned long)(unsigned int)subtype;

	if (!(type & VM_VALID_FLAG))
		return 0;

	vmap_area_list_lock();
	vmap_list = get_vmap_area_list();
	list_for_each_entry(va, vmap_list, list) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		if (!(va->flags & VM_VM_AREA))
			continue;
#endif
		if (!va->vm)
			continue;

		vm = va->vm;
		if (!(vm->flags & type))
			continue;
		len += vm->size;
	}
	vmap_area_list_unlock();
	return len;
}

size_t get_vmalloc_detail(void *buf, size_t len, void *buf_extend)
{
	size_t i;
	size_t num = len;
	size_t size;
	struct mm_vmalloc_detail_info *info = (struct mm_vmalloc_detail_info *)buf;

	for (i = 0; i < ARRAY_SIZE(vmalloc_type) && num--; i++) {
		size = vm_type_detail_get(vmalloc_type[i]);
		(info + i)->type = vmalloc_type[i];
		(info + i)->size = size;
	}
	return i;
}

/* get slub detail info */
#ifdef CONFIG_SLUB_DEBUG
size_t get_slub_detail(void *buf, size_t len, void *buf_extend)
{
	size_t cnt = 0;
	unsigned long size;
	struct kmem_cache *s = NULL;
	struct slabinfo sinfo;
	struct mm_slub_detail_info *info = (struct mm_slub_detail_info *)buf;

	mutex_lock(&slab_mutex);
	list_for_each_entry(s, &slab_caches, list) {
		if (cnt >= len) {
			mutex_unlock(&slab_mutex);
			return len;
		}
		memset(&sinfo, 0, sizeof(sinfo));
		get_slabinfo(s, &sinfo);
		size = (unsigned long)(unsigned int)s->size;
		(info + cnt)->active_objs = sinfo.active_objs;
		(info + cnt)->num_objs = sinfo.num_objs;
		(info + cnt)->objsize = (unsigned int)s->size;
		(info + cnt)->size = sinfo.num_objs * size;
		strncpy((info + cnt)->name, s->name, SLUB_NAME_LEN - 1);
		cnt++;
	}
	mutex_unlock(&slab_mutex);

	return cnt;
}

void get_slub_detail_info(void)
{
	struct kmem_cache *s = NULL;
	struct slabinfo sinfo;

	mutex_lock(&slab_mutex);
	pr_info("slab info:\n");
	pr_info("Name  Used   Total\n");
	list_for_each_entry(s, &slab_caches, list) {
		memset(&sinfo, 0, sizeof(sinfo));
		get_slabinfo(s, &sinfo);

		if (sinfo.num_objs > 0)
			pr_info("%-17s %luKB %luKB\n", s->name,
				(sinfo.active_objs * s->size) / 1024,
				(sinfo.num_objs * s->size) / 1024);
	}
	mutex_unlock(&slab_mutex);
}

#else
size_t get_slub_detail(void *buf, size_t len, void *buf_extend)
{
	pr_info("[%s] couldn't get slabinfo\n", __func__);
	return 0;
}

void get_slub_detail_info(void)
{
	pr_info("[%s] couldn't get slabinfo\n", __func__);
}

#endif

/* get memory detail info by type */
size_t mm_get_mem_detail(int type, void *buf, size_t len, void *buf_extend)
{
	unsigned int i;

	if (!buf)
		return 0;
	for (i = 0; i < ARRAY_SIZE(memdetail_ops); i++)
		if (type == memdetail_ops[i].type &&
		    memdetail_ops[i].get_mem_detail)
			return memdetail_ops[i].get_mem_detail(buf, len,
							       buf_extend);

	return 0;
}

void mm_vmalloc_detail_show(void)
{
	unsigned int i;
	size_t size;

	pr_err("========get vmalloc info start==========\n");

	for (i = 0; i < ARRAY_SIZE(vmalloc_type); i++) {
		size = vm_type_detail_get(vmalloc_type[i]);
		pr_err("vmalloc type:%s, size:%zu kB\n",
			vmalloc_text[i], size / SZ_1K);
	}

	pr_err("========get vmalloc info end==========\n");
}
