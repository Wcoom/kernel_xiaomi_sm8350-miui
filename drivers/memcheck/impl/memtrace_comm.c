/*
 * memtrace_comm.c
 *
 * Get common info function
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
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fdtable.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include "memdetail_imp.h"
#include "memtrace_comm.h"

/* get detail mem info */
size_t get_detail(void *buf, size_t len, void *buf_extend,
		  int (*f)(const void *, struct file *, unsigned int))
{
	size_t cnt = 0;
	size_t temp_num;
	struct arg_st detail_info;
	struct task_struct *tsk = NULL;
	struct mm_detail_info *info = (struct mm_detail_info *)buf;

	detail_info.used_num = 0;
	detail_info.ext_buf = buf_extend;
	if (!buf)
		return cnt;

	temp_num = snprintf(detail_info.ext_buf + detail_info.used_num,
			    EXTEND_SIZE - detail_info.used_num,
			    "Process detail info:\n");
	if (temp_num <= 0)
		return cnt;
	detail_info.used_num += min((size_t)temp_num,
				    EXTEND_SIZE - detail_info.used_num - 1);
	temp_num = snprintf(detail_info.ext_buf + detail_info.used_num,
			    EXTEND_SIZE - detail_info.used_num,
			    "-------------------------------------------\n");
	if (temp_num <= 0)
		return cnt;
	detail_info.used_num += min((size_t)temp_num,
				    EXTEND_SIZE - detail_info.used_num - 1);
	temp_num = snprintf(detail_info.ext_buf + detail_info.used_num,
			    EXTEND_SIZE - detail_info.used_num,
			    "%16s %16s %16s %50s %16s\n", "Process name",
			    "Process ID", "fd", "file_name", "size");
	if (temp_num <= 0)
		return cnt;
	detail_info.used_num += min((size_t)temp_num,
				    EXTEND_SIZE - detail_info.used_num - 1);

	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;
		if (cnt >= len) {
			rcu_read_unlock();
			return len;
		}

		task_lock(tsk);
		detail_info.tsk = tsk;
		detail_info.total_size = 0;
		iterate_fd(tsk->files, 0, f, (void *)&detail_info);
		if (detail_info.total_size) {
			(info + cnt)->size = detail_info.total_size;
			(info + cnt)->pid = tsk->pid;
			cnt++;
		}
		task_unlock(tsk);
	}
	rcu_read_unlock();

	temp_num = snprintf(detail_info.ext_buf + detail_info.used_num,
			    EXTEND_SIZE - detail_info.used_num,
			    "-------------------------------------------\n");
	if (temp_num <= 0)
		return cnt;
	detail_info.used_num += min((size_t)temp_num,
				    EXTEND_SIZE - detail_info.used_num - 1);

	return cnt;
}

size_t get_detail_by_pid(pid_t pid,
			 int (*f)(const void *, struct file *, unsigned int))
{
	struct arg_st detail_info;
	struct task_struct *tsk = NULL;
	char *buf_extend = NULL;

	buf_extend = vzalloc(EXTEND_SIZE * sizeof(*buf_extend));
	detail_info.ext_buf = buf_extend;
	detail_info.used_num = 0;
	detail_info.total_size = 0;
	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk)
		goto out;
	if (tsk->flags & PF_KTHREAD)
		goto out;

	task_lock(tsk);
	detail_info.tsk = tsk;
	iterate_fd(tsk->files, 0, f, (void *)&detail_info);
	task_unlock(tsk);
out:
	rcu_read_unlock();
	vfree(buf_extend);
	return detail_info.total_size;
}


