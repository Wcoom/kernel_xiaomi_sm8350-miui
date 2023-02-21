/*
 * memtrace_comm.h
 *
 * Get memory common info function
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

#ifndef __MEMTRACE_COMM_H
#define __MEMTRACE_COMM_H
#include <linux/sizes.h>
#include <linux/types.h>

struct arg_st {
	size_t total_size;
	struct task_struct *tsk;
	char *ext_buf;
	size_t used_num;
};
static size_t EXTEND_SIZE = (200 * 1024);

#ifdef CONFIG_DFX_MEMCHECK_EXT
size_t get_detail(void *buf, size_t len, void *buf_extend,
		  int (*f)(const void *, struct file *, unsigned int));
size_t get_detail_by_pid(pid_t pid,
			 int (*f)(const void *, struct file *, unsigned int));
#else /* CONFIG_DFX_MEMCHECK_EXT */
size_t get_detail(void *buf, size_t len, void *buf_extend,
		  int (*f)(const void *, struct file *, unsigned int))
{
	return 0;
}
size_t get_detail_by_pid(pid_t pid,
			 int (*f)(const void *, struct file *, unsigned int))
{
	return 0;
}
#endif /* CONFIG_DFX_MEMCHECK_EXT */

#endif /* __MEMTRACE_ASHMEM_H */

