
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spmi.h>

#include "internal.h"

static struct regmap *spmi_map;
static int dump_state;

int grab_spmi_map(struct regmap *map)
{
	if (map == NULL)
		return -EINVAL;
	spmi_map = map;
	return 0;
}

static int dump_prepare(char index)
{
	unsigned long value;
	int ret = -EINVAL;

	pr_info("DumpFile index %c map %p\n", index, spmi_map);
	dump_state = 0;
	if (spmi_map == NULL)
		return -EINVAL;

	if (index == '1') {
		spmi_map->dump_count = 1;
		spmi_map->dump_address = 0x1316;
		value = 0x42;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x811;
		value = 0x12;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x813;
		value = 0x12;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x815;
		value = 2;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x844;
		value = 0x00;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x845;
		value = 0x00;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x846;
		value = 0x01;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
		spmi_map->dump_address = 0x847;
		value = 0x80;
		ret = regmap_write(spmi_map, spmi_map->dump_address, value);
	}

	dump_state = ret;

	if (ret < 0) {
		pr_err("DumpFile index %c err\n", index);
		return ret;
	}

	return 0;
}

static int dump_file_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", dump_state);
	return 0;
}

static int dump_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_file_show, inode->i_private);
}

static ssize_t dump_file_write(struct file *filp,
			       const char __user *buf,
			       size_t cnt, loff_t *data)
{
	char kbuf[10];
	size_t len;

	len = min(cnt, sizeof(kbuf) -1);

	if (len > 2) {
		pr_err("dump file len err %d\n", len);
		return -EFAULT;
	}

	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;

	if (kbuf[0] >= '0' && kbuf[0] <= '9')
		dump_prepare(kbuf[0]);

	return cnt;
}

static const struct file_operations dump_file_fops = {
	.open = dump_file_open,
	.write = dump_file_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int dump_create_file(void)
{
	struct proc_dir_entry *pe;
	pe = proc_create("dump_file", 0600, NULL, &dump_file_fops);
	if (!pe)
		return -ENOMEM;

	return 0;
}
