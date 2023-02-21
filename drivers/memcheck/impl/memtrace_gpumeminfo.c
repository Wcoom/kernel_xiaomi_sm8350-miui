/*
 * memtrace_gpumeminfo.c
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

#include <linux/proc_fs.h>
#include <linux/version.h>

static int gpumem_print_entry(struct seq_file *s, void *ptr, struct task_struct *ts)
{
	struct kgsl_mem_entry *entry = ptr;
	char usage[16];
	struct kgsl_memdesc *m = &entry->memdesc;
	unsigned int usermem_type;

	usermem_type = kgsl_memdesc_usermem_type(m);
	if (usermem_type == KGSL_MEM_ENTRY_ION)
		return 0;

	kgsl_get_memory_usage(usage, sizeof(usage), m->flags);
	seq_printf(s, "%16s %6d %16llu %5d %10s %16s",
		   ts->comm, ts->tgid, m->size, entry->id,
		   memtype_str(usermem_type), usage);
	seq_putc(s, '\n');

	return 0;
}

static int gpumem_process_info_show(struct seq_file *s, void *ptr)
{
	struct kgsl_process_private *tmp = NULL;
	struct kgsl_process_private *cur_priv = NULL;

	seq_printf(s, "%16s %6s %16s %5s %10s %16s\n",
		   "task", "tgid", "size", "id", "type", "usage");

	read_lock(&kgsl_driver.proclist_lock);
	list_for_each_entry(tmp, &kgsl_driver.process_list, list) {
		int id = 0;
		struct task_struct *ts = NULL;
		struct kgsl_mem_entry *entry = NULL;

		cur_priv = tmp;
		ts = get_pid_task(cur_priv->pid, PIDTYPE_PID);
		if (!ts)
			continue;
		spin_lock(&cur_priv->mem_lock);
		idr_for_each_entry(&cur_priv->mem_idr, entry, id) {
			kgsl_mem_entry_get(entry);
			gpumem_print_entry(s, entry, ts);
			kgsl_mem_entry_put(entry);
		}
		spin_unlock(&cur_priv->mem_lock);
		put_task_struct(ts);
	}
	read_unlock(&kgsl_driver.proclist_lock);

	return 0;
}

static int gpumem_process_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, gpumem_process_info_show, PDE_DATA(inode));
}

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
static const struct proc_ops gpumem_process_info_fops = {
	.proc_open = gpumem_process_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations gpumem_process_info_fops = {
	.open = gpumem_process_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif
