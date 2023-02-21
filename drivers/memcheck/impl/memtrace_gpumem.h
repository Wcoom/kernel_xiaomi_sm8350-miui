/*
 * memtrace_gpumem.h
 *
 * Get gpumem info function
 *
 * Copyright(C) 2022 Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef __MEMTRACE_GPUMEM_H
#define __MEMTRACE_GPUMEM_H
#include <linux/sizes.h>
#include <linux/types.h>

#ifdef CONFIG_DFX_MEMCHECK_EXT
size_t mm_get_gpu_by_pid(pid_t pid);
size_t get_stats_gpu(void);
size_t gpumem_dir_info(int level, void *buff, size_t extend_len, size_t pid);
size_t gpumem_pid_detail_info(void *buff, size_t len);
#else /* CONFIG_DFX_MEMCHECK_EXT */
static inline size_t mm_get_gpu_by_pid(pid_t pid)
{
	return 0;
}
static inline size_t get_stats_gpu(void)
{
	return 0;
}
static inline size_t gpumem_dir_info(int level, void *buf, size_t len,
				     size_t pid)
{
	return 0;
}
static inline size_t gpumem_pid_detail_info(void *buf, size_t len)
{
	return 0;
}
#endif /* CONFIG_DFX_MEMCHECK_EXT */

#endif /* __MEMTRACE_GPUMEM_H */
