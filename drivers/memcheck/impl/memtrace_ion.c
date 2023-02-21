/*
 * memtrace_ion.c
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
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/printk.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/fdtable.h>
#include <linux/ion.h>
#include <linux/version.h>
#include "memtrace_ion.h"
#include "memdetail_imp.h"
#include "memtrace_comm.h"

#ifndef CONFIG_ARCH_QCOM
#include "ion_priv.h"
#elif (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
#include "ion_private.h"
#endif

static int ion_debug_process_heap_open(struct inode *inode, struct file *file);

struct ion_debug_process_heap_args {
	struct seq_file *seq;
	struct task_struct *tsk;
	size_t *total_ion_size;
};

static const struct file_operations debug_process_heap_fops = {
	.open = ion_debug_process_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dma_buf *file_to_dma_buf(struct file *file)
{
	return file->private_data;
}

/* this func must be in ion.c */
static inline struct ion_buffer *get_ion_buf(struct dma_buf *dbuf)
{
	return dbuf->priv;
}

static int get_ion_dbuf_size(const void *data, struct file *f, unsigned int fd)
{
	struct dma_buf *dbuf = NULL;
	struct ion_buffer *ibuf = NULL;
	struct arg_st *arg = (struct arg_st *)data;
	size_t temp_num;

	if (!is_dma_buf_file(f))
		return 0;

	dbuf = file_to_dma_buf(f);
	if (!dbuf)
		return 0;

	if (!is_ion_dma_buf(dbuf))
		return 0;

	if (dbuf->owner != THIS_MODULE)
		return 0;

	ibuf = get_ion_buf(dbuf);
	if (!ibuf)
		return 0;

	arg->total_size += dbuf->size;
	temp_num = snprintf(arg->ext_buf + arg->used_num,
			    EXTEND_SIZE - arg->used_num,
			    "%16s %16u %16u %50s %16u\n",
			    arg->tsk->comm, arg->tsk->pid, fd, dbuf->exp_name,
			    dbuf->size);
	if (temp_num <= 0)
		return 0;
	arg->used_num += min((size_t)temp_num, EXTEND_SIZE - arg->used_num - 1);

	return 0;
}

/* get ion detail mem info */
size_t get_ion_detail(void *buf, size_t len, void *buf_extend)
{
	return get_detail(buf, len, buf_extend, get_ion_dbuf_size);
}

size_t mm_get_ion_by_pid(pid_t pid)
{
	return get_detail_by_pid(pid, get_ion_dbuf_size);
}

size_t get_stats_ion(void)
{
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	return ion_get_total_heap_bytes();
#else
	return get_ion_total();
#endif
}

static int ion_debug_process_heap_cb(const void *data, struct file *f,
				     unsigned int fd)
{
	const struct ion_debug_process_heap_args *args = data;
	struct task_struct *tsk = args->tsk;
	struct dma_buf *dbuf = NULL;

	if (!is_dma_buf_file(f))
		return 0;

	dbuf = file_to_dma_buf(f);
	if (!dbuf)
		return 0;

	if (dbuf->owner != THIS_MODULE)
		return 0;

	*args->total_ion_size += dbuf->size;
	seq_printf(args->seq, "%s %u %u %zu %u %u %u %-.16s\n",
		tsk->comm, tsk->pid, fd, dbuf->size,
		to_msm_dma_buf(dbuf)->i_ino, dbuf->tgid,
		dbuf->pid, dbuf->exp_name);

	return 0;
}

static int ion_process_heap_cb(const void *data, struct file *f,
			       unsigned int fd)
{
	const struct ion_debug_process_heap_args *args = data;
	struct task_struct *tsk = args->tsk;
	struct dma_buf *dbuf = NULL;

	if (!is_dma_buf_file(f))
		return 0;

	dbuf = file_to_dma_buf(f);
	if (!dbuf)
		return 0;

	if (dbuf->owner != THIS_MODULE)
		return 0;

	*args->total_ion_size += dbuf->size;
	pr_info("%s %u %u %zu %u %u %u %-.16s\n",
		tsk->comm, tsk->pid, fd, dbuf->size,
		to_msm_dma_buf(dbuf)->i_ino, dbuf->tgid,
		dbuf->pid, dbuf->exp_name);

	return 0;
}

static int ion_debug_process_heap_show(struct seq_file *s, void *d)
{
	struct task_struct *tsk = NULL;
	size_t task_total_ion_size;
	struct ion_debug_process_heap_args cb_args;
	size_t total_ion_size = 0;

	seq_puts(s, "Process ION heap info:\n");
	seq_puts(s, "----------------------------------------------------\n");
	seq_printf(s, "%s %s %s %s %s %s %s %-.16s\n",
			"Process name", "Process ID",
			"fd", "size", "magic", "buf->tgid",
			"buf->pid", "buf->task_comm");

	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		task_total_ion_size = 0;
		cb_args.seq = s;
		cb_args.tsk = tsk;
		cb_args.total_ion_size = &task_total_ion_size;

		task_lock(tsk);
		iterate_fd(tsk->files, 0, ion_debug_process_heap_cb,
			   (void *)&cb_args);
		total_ion_size += task_total_ion_size;
		task_unlock(tsk);
	}
	rcu_read_unlock();

	seq_puts(s, "----------------------------------------------------\n");
	return 0;
}

void ion_heap_show(void)
{
	struct task_struct *tsk = NULL;
	size_t task_total_ion_size;
	struct ion_debug_process_heap_args cb_args;
	size_t total_ion_size = 0;

	pr_info("Process ion heap info:\n");
	pr_info("---------------------------------------------------------\n");
	pr_info("%s %s %s %s %s %s %s %-.16s\n",
		"Process name", "Process ID",
		"fd", "size", "magic", "buf->tgid",
		"buf->pid", "buf->task_comm");

	rcu_read_lock();
	for_each_process(tsk) {
		if (tsk->flags & PF_KTHREAD)
			continue;

		task_total_ion_size = 0;
		cb_args.tsk = tsk;
		cb_args.total_ion_size = &task_total_ion_size;

		task_lock(tsk);
		iterate_fd(tsk->files, 0, ion_process_heap_cb, (void *)&cb_args);
		total_ion_size += task_total_ion_size;
		task_unlock(tsk);
	}
	rcu_read_unlock();

	pr_info("----------------------------------------------------------\n");
}
EXPORT_SYMBOL(ion_heap_show);

static int ion_debug_process_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_process_heap_show, inode->i_private);
}

static int ion_memtrack_show(struct seq_file *s, void *d)
{
	struct dma_buf *d_buf = NULL;
	struct dma_buf_list *d_buf_list = get_dma_buf_list();

	mutex_lock(&d_buf_list->lock);
	list_for_each_entry(d_buf, &d_buf_list->head, list_node) {
		if (d_buf == NULL || d_buf->priv == NULL)
			continue;

		if (mutex_lock_interruptible(&d_buf->lock)) {
			pr_info("error locking dma buffer object: skipping\n");
			continue;
		}

		if (!is_ion_dma_buf(d_buf)) {
			mutex_unlock(&d_buf->lock);
			continue;
		}

		seq_printf(s, "%16.s %16u %16zu\n", d_buf->exp_name,
				d_buf->tgid, d_buf->size);
		mutex_unlock(&d_buf->lock);
	}
	mutex_unlock(&d_buf_list->lock);

	return 0;
}

static int ion_memtrack_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_memtrack_show, PDE_DATA(file_inode(file)));
}

static const struct file_operations memtrack_fops = {
	.open = ion_memtrack_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void mm_ion_process_info(void *idev)
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data("ion_process_info", 0444,
		NULL, &debug_process_heap_fops, idev);
	if (!entry)
		pr_err("Failed to create ion buffer debug info\n");

	entry = proc_create_data("memtrack", 0444,
		NULL, &memtrack_fops, idev);
	if (!entry)
		pr_err("Failed to create heap debug memtrack\n");
}

