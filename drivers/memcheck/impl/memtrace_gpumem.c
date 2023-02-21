/*
 * memtrace_gpumem.c
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
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fdtable.h>
#include <linux/sched/signal.h>
#include <linux/version.h>
#include "memtrace_comm.h"
#include "../memcheck_ioctl.h"
#include "memdetail_imp.h"

#if defined(CONFIG_ARCH_QCOM)
#define GPUMEM_PATH "/sys/devices/virtual/kgsl/kgsl"
#elif defined(CONFIG_ARCH_MTK)
#define GPUMEM_PATH "/sys/kernel/debug/mali0/gpu_memory"
#elif defined(CONFIG_ARCH_HISI)
#define GPUMEM_PATH "/proc/gpu_memory"
#endif

#define DT_DIR 4
#define DECIMAL 10
#define CUR_DIR_LEN 1
#define UP_DIR_LEN 2
#define ALL_PID_INFO_SIZE 1000
#define FILE_SIZE_LEN 20
#define FILE_BUFFER_SIZE 128
static char cur_path[FILE_BUFFER_SIZE];
static char tab_buf[FILE_BUFFER_SIZE];
static char *black_list[] = {
	"proc",
	"subsystem",
};

struct readdir_cb {
	struct dir_context ctx;
	int result;
	unsigned int is_dir;
};

static int gpupid_info_filldir(struct dir_context *ctx, const char *name,
			       int namelen, loff_t offset, u64 ino,
			       unsigned int d_type)
{
	struct readdir_cb *buf = container_of(ctx, struct readdir_cb, ctx);
	int name_buf_len = namelen + 1;

	if (buf->result)
		return -EINVAL;

	if (name_buf_len > FILE_BUFFER_SIZE) {
		buf->result = -ENOMEM;
		return -ENOMEM;
	}
	buf->result++;
	buf->is_dir = d_type;
	strncpy(cur_path, name, name_buf_len);

	return 0;
}

static int file_read_dir(struct file *filp, int *is_dir)
{
	int ret;
	struct readdir_cb data_cb = {
		.ctx.actor = gpupid_info_filldir,
	};

	if (IS_ERR(filp))
		return -EBADF;
	ret = iterate_dir(filp, &data_cb.ctx);
	if (data_cb.result)
		ret = data_cb.result;
	*is_dir = data_cb.is_dir;

	return ret;
}

static void add_tab_buf(size_t level)
{
	int i;

	for (i = 0; i < (level + 1); i++)
		tab_buf[i] = '	';
	tab_buf[i] = '\0';
}

static bool is_black_list(char *dir_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(black_list); i++) {
		if (!strcmp(dir_name, black_list[i]))
			return true;
	}

	return false;
}

static void save_file_size(char *path, int level, void *buff, size_t len,
			   size_t *write_num)
{
	size_t temp_num;
	struct file *cur_filp = NULL;
	char data[ALL_PID_INFO_SIZE] = {0};
	mm_segment_t old_fs;

	cur_filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(cur_filp)) {
		memcheck_err("open path: %s failed!\n", path);
		return;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	cur_filp->f_pos = 0;

	memset(data, 0, sizeof(data));
	temp_num = vfs_read(cur_filp, data, sizeof(data) - 2,
			    &cur_filp->f_pos);
	set_fs(old_fs);
	if (temp_num <= 0) {
		memcheck_err("read %s file size failed!\n", path);
		data[0] = '\n';
	}
	if (data[strlen(data) - 1] != '\n')
		data[strlen(data)] = '\n';
	if (!IS_ERR(cur_filp))
		filp_close(cur_filp, NULL);
	add_tab_buf(level + 1);
	temp_num = snprintf(buff + *write_num,
				len - *write_num, "%s%20s : %16s",
				tab_buf, cur_path, data);
	if (temp_num <= 0)
		return;
	*write_num += min((size_t)temp_num, len - *write_num - 1);
}

static void gpumem_dir_detail_info(char *path, int level, void *buff,
				   size_t len, size_t *write_num)
{
	struct file *filp = NULL;
	unsigned int is_dir = 0;
	char new_path[FILE_BUFFER_SIZE] = {0};
	size_t temp_num;

	if (level > 3) // 3 is max dir depth
		return;

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		memcheck_err("open path: %s failed!\n", path);
		return;
	}

	while (file_read_dir(filp, &is_dir) > 0) {
		if (strncmp(cur_path, ".", CUR_DIR_LEN) == 0)
			continue;
		if (strncmp(cur_path, "..", UP_DIR_LEN) == 0)
			continue;

		strncpy(new_path, path, sizeof(new_path));
		temp_num = snprintf(new_path, sizeof(new_path), "%s/%s", path,
				    cur_path);
		if (temp_num <= 0) {
			memcheck_err("invalid pid: %d\n", new_path);
			continue;
		}

		if (is_dir == DT_DIR) {
			if (is_black_list(cur_path))
				continue;
			add_tab_buf(level + 1);
			temp_num = snprintf(buff + *write_num, len - *write_num,
					    "%s%s\n", tab_buf, cur_path);
			if (temp_num <= 0)
				continue;
			*write_num += min((size_t)temp_num,
					  len - *write_num - 1);
			level++;

			gpumem_dir_detail_info(new_path, level, buff, len,
					       write_num);
			level--;

		} else {
			save_file_size(new_path, level, buff, len, write_num);
		}
	}

	if (!IS_ERR(filp))
		filp_close(filp, NULL);
}

size_t gpumem_dir_info(int level, void *buff, size_t extend_len, size_t pid)
{
	size_t write_num = 0;
	size_t temp_num;

	temp_num = snprintf(buff + write_num, extend_len - write_num,
			    "gpu detail info\n");
	if (temp_num <= 0)
		return write_num;
	write_num += min((size_t)temp_num, extend_len - write_num - 1);
	temp_num = snprintf(buff + write_num, extend_len - write_num,
			    "-------------------------------------------\n");
	if (temp_num <= 0)
		return write_num;
	write_num += min((size_t)temp_num, extend_len - write_num - 1);
	temp_num = snprintf(buff + write_num, extend_len - write_num,
			    "%20s : %16s\n", "File name", " size");
	if (temp_num <= 0)
		return write_num;
	write_num += min((size_t)temp_num, extend_len - write_num - 1);

	gpumem_dir_detail_info(GPUMEM_PATH, level, buff, extend_len,
				&write_num);

	temp_num = snprintf(buff + write_num, extend_len - write_num,
			    "------------------------------------------\n");
	if (temp_num <= 0)
		return write_num;
	write_num += min((size_t)temp_num, extend_len - write_num - 1);

	return write_num;
}

static int read_gpu_file(char *data, int len)
{
	int num;
	int ret = -EFAULT;
	struct file *filp = NULL;
	char gpumem_path[FILE_BUFFER_SIZE] = {0};
	mm_segment_t old_fs;

	num = snprintf(gpumem_path, sizeof(gpumem_path), "%s/%s",
		       GPUMEM_PATH, "unmapped_single_detail");
	if (num <= 0) {
		memcheck_err("invalid file: %s\n", gpumem_path);
		return ret;
	}

	filp = filp_open(gpumem_path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		memcheck_err("open %s file error\n", gpumem_path);
		return ret;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filp->f_pos = 0;
	num = vfs_read(filp, data, len, &filp->f_pos);
	set_fs(old_fs);
	if (num <= 0) {
		memcheck_err("read %s file error, num: %d\n", gpumem_path,
			     num);
		goto out;
	}
	data[len - 1] = 0;
	ret = 0;
out:
	if (!IS_ERR(filp))
		filp_close(filp, NULL);

	return ret;
}

static int read_pid_and_size(char *data, char *const pid, char *const size, int len)
{
	char *cur = data;
	char *token = NULL;
	int i = 0;
	int ret = -EFAULT;

	while ((token = strsep(&cur, " ")) != NULL) {
		if (token[0] == 0)
			continue;
		else if (i == 1) // 1 indicates the first column is pid name
			strncpy(pid, token, len);
		else if (i == 3) // 3 indicates the second column is size
			strncpy(size, token, len);
		i++;
	}

	if (i > 3) // one row has at least three columns
		ret = 0;

	return ret;
}

size_t mm_get_gpu_by_pid(pid_t pid)
{
	int cur_pid;
	int total_size = 0;
	char data_buff[ALL_PID_INFO_SIZE] = {0};
	char pid_name[FILE_SIZE_LEN] = {0};
	char size[FILE_SIZE_LEN] = {0};
	char *cur = NULL;
	char *token = NULL;

	if (read_gpu_file(data_buff, ALL_PID_INFO_SIZE))
		return total_size;

	cur = data_buff;
	while ((token = strsep(&cur, "\n")) != NULL) {
		memset(pid_name, 0, sizeof(pid_name));
		memset(size, 0, sizeof(size));

		if (!read_pid_and_size(token, pid_name, size, FILE_SIZE_LEN)) {
			if (kstrtoint(pid_name, DECIMAL, &cur_pid))
				continue;
			if (cur_pid != pid)
				continue;
			if (!kstrtoint(size, DECIMAL, &total_size))
				return total_size * PAGE_SIZE;
		}
	}

	memcheck_info("this pid: %d have not gpu mem!\n", pid);
	return total_size * PAGE_SIZE;
}

size_t gpumem_pid_detail_info(void *buff, size_t len)
{
	int pid;
	int pid_gpu_size;
	size_t cnt = 0;
	char data_buff[ALL_PID_INFO_SIZE] = {0};
	char pid_name[FILE_SIZE_LEN] = {0};
	char size[FILE_SIZE_LEN] = {0};
	char *cur = NULL;
	char *token = NULL;
	struct mm_detail_info *info = (struct mm_detail_info *)buff;

	if (!buff)
		return cnt;

	if (read_gpu_file(data_buff, ALL_PID_INFO_SIZE))
		return cnt;

	cur = data_buff;
	while ((token = strsep(&cur, "\n")) != NULL) {
		memset(pid_name, 0, sizeof(pid_name));
		memset(size, 0, sizeof(size));
		if (!read_pid_and_size(token, pid_name, size, FILE_SIZE_LEN)) {
			if ((!kstrtoint(pid_name, DECIMAL, &pid)) &&
			    (!kstrtoint(size, DECIMAL, &pid_gpu_size))) {
				(info + cnt)->pid = pid;
				(info + cnt)->size = pid_gpu_size * PAGE_SIZE;
				cnt++;
				if (cnt > len)
					break;
			}
		}
	}

	return cnt;
}

static size_t gpumem_get_total_size(void)
{
	size_t total_size = 0;
	int tmp_size = 0;
	char data_buff[ALL_PID_INFO_SIZE] = {0};
	char pid_name[FILE_SIZE_LEN] = {0};
	char size[FILE_SIZE_LEN] = {0};
	char *cur = NULL;
	char *token = NULL;

	if (read_gpu_file(data_buff, ALL_PID_INFO_SIZE))
		return total_size;

	cur = data_buff;
	while ((token = strsep(&cur, "\n")) != NULL) {
		memset(pid_name, 0, sizeof(pid_name));
		memset(size, 0, sizeof(size));
		if (!read_pid_and_size(token, pid_name, size, FILE_SIZE_LEN)) {
			if (!kstrtoint(size, DECIMAL, &tmp_size))
				total_size += tmp_size;
		}
	}

	return total_size * PAGE_SIZE;
}

size_t get_stats_gpu(void)
{
	return gpumem_get_total_size();
}
