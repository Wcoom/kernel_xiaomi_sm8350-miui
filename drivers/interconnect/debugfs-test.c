// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017, Linaro Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/interconnect.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

static struct platform_device *icc_pdev;
static struct dentry *debugfs_dir;
static struct icc_path *path;
static u32 src_port;
static u32 dst_port;
static u32 avg_bw;
static u32 peak_bw;

#ifdef CONFIG_CX_POWER_OPTIMIZE
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>

extern unsigned int enable_cx_decrease;
#define MLEVEL_BW_UTILIZATION 1
#define HLEVEL_BW_UTILIZATION 2
static unsigned long enable_bw = 0;

static ssize_t bw_proc_write(struct file *file, char const __user *buf,
			    size_t count, loff_t *ppos)
{
	int ret;

	ret = kstrtoul_from_user(buf, count, 10, &enable_bw);
	pr_err("[bw_write] enable_bw = %lu \n", enable_bw);

	if (ret)
		return ret;

	if (enable_bw == MLEVEL_BW_UTILIZATION)
		enable_cx_decrease = MLEVEL_BW_UTILIZATION;
	else if (enable_bw == HLEVEL_BW_UTILIZATION)
		enable_cx_decrease = HLEVEL_BW_UTILIZATION;
	else
		enable_cx_decrease = 0;

	*ppos += count;

	return count;
}

static int bw_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m,"%d\n",enable_bw);
	return 0;
}

static int bw_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bw_proc_show, NULL);
}

static const struct file_operations bw_opt_fops = {
	.owner = THIS_MODULE,
	.open = bw_proc_open,
	.write = bw_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif

static ssize_t get_write_op(struct file *file, char const __user *buf,
			    size_t count, loff_t *ppos)
{
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(buf, count, 10, &val);
	if (ret)
		return ret;

	path = icc_get(&icc_pdev->dev, src_port, dst_port);
	if (IS_ERR(path))
		return PTR_ERR(path);

	*ppos += count;

	return count;
}

static const struct file_operations get_fops = {
	.owner = THIS_MODULE,
	.write = get_write_op
};

static ssize_t commit_write_op(struct file *file, char const __user *buf,
			       size_t count, loff_t *ppos)
{
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(buf, count, 10, &val);
	if (ret)
		return ret;

	if (IS_ERR(path))
		return PTR_ERR(path);

	ret = icc_set_bw(path, avg_bw, peak_bw);
	if (ret)
		return ret;

	*ppos += count;

	return count;
}

static const struct file_operations commit_fops = {
	.owner = THIS_MODULE,
	.write = commit_write_op
};

static void __exit icc_test_exit(void)
{
	if (!IS_ERR(path))
		icc_put(path);

	debugfs_remove_recursive(debugfs_dir);
	platform_device_del(icc_pdev);
	platform_device_put(icc_pdev);
}

static int __init icc_test_init(void)
{
	icc_pdev = platform_device_alloc("icc-test", PLATFORM_DEVID_AUTO);
	platform_device_add(icc_pdev);

	debugfs_dir = debugfs_create_dir("interconnect-test", NULL);
	if (!debugfs_dir)
		pr_err("interconnect: error creating debugfs directory\n");

	debugfs_create_u32("src_port", 0600, debugfs_dir, &src_port);
	debugfs_create_u32("dst_port", 0600, debugfs_dir, &dst_port);
	debugfs_create_file("get", 0200, debugfs_dir, NULL, &get_fops);
	debugfs_create_u32("avg_bw", 0600, debugfs_dir, &avg_bw);
	debugfs_create_u32("peak_bw", 0600, debugfs_dir, &peak_bw);
	debugfs_create_file("commit", 0200, debugfs_dir, NULL, &commit_fops);

#ifdef CONFIG_CX_POWER_OPTIMIZE
	if (!proc_create("enable_bw_optimize", 0644, NULL, &bw_opt_fops))
		pr_err("proc/enable_bw_optimize create fail\n");
#endif

	return 0;
}

module_init(icc_test_init);
module_exit(icc_test_exit);
MODULE_LICENSE("GPL v2");

#endif
