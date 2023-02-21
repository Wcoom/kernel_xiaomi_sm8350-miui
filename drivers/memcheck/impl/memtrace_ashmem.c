/*
 * memtrace_ashmem.c
 *
 * Get ashmem info function
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
#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/printk.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include <linux/version.h>
#include "ashmem.h"
#include "memtrace_comm.h"
#define ASHMEM_DATA  0

static int ashmem_debug_process_info_open(struct inode *inode, struct file *file);

struct ashmem_debug_process_info_args {
	struct seq_file *seq;
	struct task_struct *tsk;
	size_t *total_ashmem_size;
};

static const struct file_operations debug_process_ashmem_info_fops = {
	.open = ashmem_debug_process_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/* accumulate ashmem total size */
static int get_ashmem_size(const void *data, struct file *f, unsigned int fd)
{
	struct arg_st *args = (struct arg_st *)data;
	size_t temp_num;

	if (!is_ashmem(f))
		return 0;

	args->total_size += get_ashmem_size_by_file(f);

	temp_num = snprintf(args->ext_buf + args->used_num,
			    EXTEND_SIZE - args->used_num,
			    "%16s %16u %16u %50s: %16u\n", args->tsk->comm,
			    args->tsk->pid, fd, get_ashmem_name_by_file(f),
			    get_ashmem_size_by_file(f));
	if (temp_num <= 0)
		return 0;
	args->used_num += min((size_t)temp_num,
			      EXTEND_SIZE - args->used_num - 1);

	return 0;
}

/* get ashmem detail mem info */
size_t get_ashmem_detail(void *buf, size_t len, void *buf_extend)
{
	size_t total_size;

	ashmem_mutex_lock();
	total_size = get_detail(buf, len, buf_extend, get_ashmem_size);
	ashmem_mutex_unlock();
	return total_size;
}

size_t mm_get_ashmem_by_pid(pid_t pid)
{
	size_t total_size;

	ashmem_mutex_lock();
	total_size = get_detail_by_pid(pid, get_ashmem_size);
	ashmem_mutex_unlock();
	return total_size;
}

size_t get_stats_ashmem(void)
{
	return ashmem_get_total_size();
}

static int ashmem_debug_process_info_cb(const void *data,
					struct file *f, unsigned int fd)
{
	const struct ashmem_debug_process_info_args *args = data;
	struct task_struct *tsk = args->tsk;

	if (!is_ashmem(f))
		return 0;

	seq_printf(args->seq,
		"%s %u %u %s %u\n",
		tsk->comm, tsk->pid, fd, get_ashmem_name_by_file(f), get_ashmem_size_by_file(f));

	return 0;
}

static int ashmem_process_info_cb(const void *data,
					struct file *f, unsigned int fd)
{
	const struct ashmem_debug_process_info_args *args = data;
	struct task_struct *tsk = args->tsk;

	if (!is_ashmem(f))
		return 0;

	pr_info("%s %u %u %s %u\n",
		tsk->comm, tsk->pid, fd, get_ashmem_name_by_file(f), get_ashmem_size_by_file(f));

	return 0;
}

static int ashmem_debug_process_info_show(struct seq_file *s, void *d)
{
	struct task_struct *tsk = NULL;
	struct ashmem_debug_process_info_args cb_args;

	seq_puts(s, "Process ashmem detail info:\n");
	seq_puts(s, "----------------------------------------------------\n");
	seq_printf(s, "%s %s %s %s %s\n",
		"Process name", "Process ID",
		"fd", "ashmem_name", "size");

	ashmem_mutex_lock();
	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		cb_args.seq = s;
		cb_args.tsk = tsk;

		task_lock(tsk);
		iterate_fd(tsk->files, 0, ashmem_debug_process_info_cb, (void *)&cb_args);
		task_unlock(tsk);
	}
	rcu_read_unlock();
	ashmem_mutex_unlock();
	seq_puts(s, "----------------------------------------------------\n");
	return 0;
}

void ashmem_info_show(void)
{
	struct task_struct *tsk = NULL;
	struct ashmem_debug_process_info_args cb_args;

	pr_info("Process ashmem detail info:\n");
	pr_info("----------------------------------------------------------\n");
	pr_info("%s %s %s %s %s\n",
		"Process name", "Process ID",
		"fd", "ashmem_name", "size");

	ashmem_mutex_lock();
	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		cb_args.tsk = tsk;

		task_lock(tsk);
		iterate_fd(tsk->files, 0,
			    ashmem_process_info_cb, (void *)&cb_args);
		task_unlock(tsk);
	}
	rcu_read_unlock();
	ashmem_mutex_unlock();
}
EXPORT_SYMBOL(ashmem_info_show);

static int ashmem_debug_process_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ashmem_debug_process_info_show, inode->i_private);
}

void mm_ashmem_process_info(void)
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data("ashmem_process_info", 0444,
		NULL, &debug_process_ashmem_info_fops, (void *)ASHMEM_DATA);
	if (!entry)
		pr_err("Failed to create ashmem debug info\n");
}
