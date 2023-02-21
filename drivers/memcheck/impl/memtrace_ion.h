/*
 * memtrace_ion.h
 *
 * Get ION memory info function
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

#ifndef __MEMTRACE_ION_H
#define __MEMTRACE_ION_H
#include <linux/sizes.h>
#include <linux/types.h>

#ifdef CONFIG_DFX_MEMCHECK_EXT
size_t mm_get_ion_by_pid(pid_t pid);
size_t get_ion_detail(void *buf, size_t len, void *buf_extend);
size_t get_stats_ion(void);
#else /* CONFIG_DFX_MEMCHECK_EXT */
static inline size_t mm_get_ion_by_pid(pid_t pid)
{
	return 0;
}
static inline size_t get_ion_detail(void *buf, size_t len, void *buf_extend)
{
	return 0;
}
static inline size_t get_stats_ion(void)
{
	return 0;
}
#endif /* CONFIG_DFX_MEMCHECK_EXT */

#endif /* __MEMTRACE_ION_H */

