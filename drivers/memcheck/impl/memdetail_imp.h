/*
 * memdetail_imp.h
 *
 * Get detailed memory info functions
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

#ifndef __MEMDETAIL_IMP_H
#define __MEMDETAIL_IMP_H
#include <linux/sizes.h>
#include <linux/types.h>

#define SLUB_NAME_LEN  64

struct mm_slub_detail_info {
	char name[SLUB_NAME_LEN];
	unsigned long active_objs;
	unsigned long num_objs;
	unsigned long active_slabs;
	unsigned long num_slabs;
	unsigned long size; /* total size */
	unsigned int objects_per_slab;
	unsigned int objsize;
};

struct mm_detail_info {
	pid_t pid;
	size_t size;
};

struct mm_vmalloc_detail_info {
	int type;
	size_t size;
};

size_t get_vmalloc_detail(void *buf, size_t len, void *buf_extend);
size_t get_slub_detail(void *buf, size_t len, void *buf_extend);

#ifdef CONFIG_DFX_MEMCHECK_DETAIL
size_t mm_get_mem_detail(int type, void *buf, size_t len, void *buf_extend);
size_t vm_type_detail_get(int subtype);
#else
static inline size_t mm_get_mem_detail(int type, void *buf, size_t len,
				       void *buf_extend)
{
	return 0;
}
static inline size_t vm_type_detail_get(int subtype)
{
	return 0;
}
#endif
#endif /* __MEMDETAIL_IMP_H */
