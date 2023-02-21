/*
 * memcheck_detail.c
 *
 * save and read detailed information from native or java process, send signal to them
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "memcheck_detail.h"
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/sort.h>
#include <linux/version.h>
#include <linux/thread_info.h>
#if (KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE)
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif
#include "memcheck_memstat.h"
#include "impl/memtrace_comm.h"

/* for detail information */
#define DETAIL_SIZE	(100 * 1024)

/* for native detail information save and read */
#define NATIVE_DETAIL_WAIT_TIME_SEC	5
static DEFINE_MUTEX(native_detail_mutex);
static void *native_detail_buf;
static u64 native_detail_len;
static wait_queue_head_t native_detail_ready;
static bool is_waiting;

static int slub_cmp(const void *a, const void *b)
{
	const struct mm_slub_detail_info *info1 = a;
	const struct mm_slub_detail_info *info2 = b;

	if (info1->size < info2->size)
		return 1;
	else if (info1->size > info2->size)
		return -1;
	else
		return 0;
}

static int memcheck_slub_detail_read(size_t num, char *buf, size_t total)
{
	struct mm_slub_detail_info *list = NULL;
	size_t ret_num;
	size_t used = 0;
	int tmp;
	int i;

	list = vzalloc(num * sizeof(*list));
	if (!list)
		return 0;
	ret_num = mm_get_mem_detail(SLUB_TRACK, list, num, NULL);
	if (ret_num == 0)
		goto err_list;

	/* sort the list */
	sort(list, ret_num, sizeof(*list), slub_cmp, NULL);

	/* save the top slub occupation name */
	memcheck_save_top_slub(list->name);

	/* convert to string */
	tmp = snprintf(buf, total,
		       "name	active_objs	num_objs	objsize	total_size\n");
	if (tmp < 0)
		goto err_list;
	used = min((size_t)tmp, total - 1);
	for (i = 0; i < ret_num; i++) {
		tmp =
		    snprintf(buf + used, total - used, "%s %lu %lu %lu %lu\n",
			     list[i].name, list[i].active_objs,
			     list[i].num_objs, list[i].objsize,
			     list[i].size);
		if (tmp < 0)
			goto err_list;
		used += min((size_t)tmp, total - used - 1);
		if (used >= (total - 1))
			break;
	}

err_list:
	vfree(list);

	return used;
}

int memcheck_slub_detail_preread(void)
{
	struct mm_slub_detail_info *list = NULL;
	size_t ret_num;

	list = vzalloc(DETAIL_SIZE);
	if (!list)
		return -ENOMEM;
	ret_num = mm_get_mem_detail(SLUB_TRACK, list,
				    DETAIL_SIZE / sizeof(*list), NULL);
	if (ret_num == 0) {
		vfree(list);
		return -EFAULT;
	}

	sort(list, ret_num, sizeof(*list), slub_cmp, NULL);
	memcheck_save_top_slub(list->name);
	vfree(list);

	return 0;
}

static int mm_size_cmp(const void *a, const void *b)
{
	const struct mm_detail_info *info1 = a;
	const struct mm_detail_info *info2 = b;

	if (info1->size < info2->size)
		return 1;
	else if (info1->size > info2->size)
		return -1;
	else
		return 0;
}

static int memcheck_ion_ashmem_detail_read(size_t num, char *buf, size_t total,
					   int type)
{
	struct mm_detail_info *list = NULL;
	size_t ret_num;
	size_t used = 0;
	int tmp;
	int i;
	char *buf_extend = NULL;

	list = vzalloc(num * sizeof(*list));
	if (!list)
		return 0;
	buf_extend = vzalloc(EXTEND_SIZE * sizeof(*buf_extend));
	if (!buf_extend) {
		vfree(list);
		return 0;
	}

	ret_num = mm_get_mem_detail(type, list, num, buf_extend);
	if (ret_num == 0)
		goto err_list;

	/* sort the list */
	sort(list, ret_num, sizeof(*list), mm_size_cmp, NULL);

	/* convert to string */
	tmp = snprintf(buf, total, "pid	size\n");
	if (tmp < 0)
		goto err_list;
	used = min((size_t)tmp, total - 1);
	for (i = 0; i < ret_num; i++) {
		tmp = snprintf(buf + used, total - used, "%d %lu\n",
			       list[i].pid, list[i].size);
		if (tmp < 0)
			goto err_list;
		used += min((size_t)tmp, total - used - 1);
		if (used >= (total - 1))
			break;
	}

	tmp = snprintf(buf + used, total - used, "%s\n", buf_extend);
	if (tmp < 0)
		goto err_list;
	used += min((size_t)tmp, total - used - 1);

err_list:
	vfree(list);
	vfree(buf_extend);

	return used;
}

static int memcheck_gpu_detail_read(size_t num, char *buf, size_t total)
{
	struct mm_detail_info *tmp_list = NULL;
	size_t ret_num;
	size_t used = 0;
	int tmp;
	int i;

	tmp_list = vzalloc(num * sizeof(*tmp_list));
	if (!tmp_list)
		return 0;
	ret_num = gpumem_pid_detail_info(tmp_list, num);
	if (ret_num == 0)
		goto err_list;

	/* sort the list */
	sort(tmp_list, ret_num, sizeof(*tmp_list), mm_size_cmp, NULL);

	/* convert to string */
	tmp = snprintf(buf, total, "pid size\n");
	if (tmp < 0)
		goto err_list;
	used = min((size_t)tmp, total - 1);
	for (i = 0; i < ret_num; i++) {
		tmp = snprintf(buf + used, total - used, "%d %lu\n",
			       tmp_list[i].pid, tmp_list[i].size);
		if (tmp < 0)
			goto err_list;
		used += min((size_t)tmp, total - used - 1);
		if (used >= (total - 1))
			break;
	}

err_list:
	vfree(tmp_list);

	return used;
}

static int vmalloc_cmp(const void *a, const void *b)
{
	const struct mm_vmalloc_detail_info *info1 = a;
	const struct mm_vmalloc_detail_info *info2 = b;

	if (info1->size < info2->size)
		return 1;
	else if (info1->size > info2->size)
		return -1;
	else
		return 0;
}

static const char *type_to_str(int type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vmalloc_type); i++) {
		if (vmalloc_type[i] == type)
			return vmalloc_text[i];
	}
	return "NA";
}

static int memcheck_vmalloc_detail_read(size_t num, char *buf, size_t total)
{
	struct mm_vmalloc_detail_info *list = NULL;
	size_t ret_num;
	size_t used = 0;
	int tmp;
	int i;

	list = vzalloc(num * sizeof(*list));
	if (!list)
		return 0;
	ret_num = mm_get_mem_detail(VMALLOC_TRACK, list, num, NULL);
	if (ret_num == 0)
		goto err_list;

	/* sort the list */
	sort(list, ret_num, sizeof(*list), vmalloc_cmp, NULL);

	/* convert to string */
	tmp = snprintf(buf, total, "type	size\n");
	if (tmp < 0)
		goto err_list;
	used = min((size_t)tmp, total - 1);
	for (i = 0; i < ret_num; i++) {
		tmp = snprintf(buf + used, total - used, "%s %lu\n",
			       type_to_str(list[i].type), list[i].size);
		if (tmp < 0)
			goto err_list;
		used += min((size_t)tmp, total - used - 1);
		if (used >= (total - 1))
			break;
	}

err_list:
	vfree(list);

	return used;
}

int memcheck_native_detail_write(const void *buf,
				 const struct detail_info *info)
{
	char *tmp = NULL;

	tmp = vzalloc(info->size + 1);
	if (!tmp)
		return -EFAULT;
	if (copy_from_user(tmp, buf + sizeof(*info), info->size)) {
		vfree(tmp);
		memcheck_err("copy_from_user failed\n");
		return -EFAULT;
	}
	tmp[info->size] = 0;

	mutex_lock(&native_detail_mutex);
	if (native_detail_buf)
		vfree(native_detail_buf);
	native_detail_buf = tmp;
	native_detail_len = info->size;
	mutex_unlock(&native_detail_mutex);

	if (is_waiting)
		wake_up_interruptible(&native_detail_ready);

	return 0;
}

static int memcheck_wait_native_detail_ready(void)
{
	int ret;

	mutex_lock(&native_detail_mutex);
	is_waiting = true;
	init_waitqueue_head(&native_detail_ready);
	mutex_unlock(&native_detail_mutex);
	ret = wait_event_interruptible_timeout(native_detail_ready,
					native_detail_buf,
					NATIVE_DETAIL_WAIT_TIME_SEC * HZ);
	mutex_lock(&native_detail_mutex);
	if (native_detail_buf && ret > 0) {
		memcheck_info("get native detail info successfully\n");
		ret = 0;
	} else if (!ret) {
		memcheck_err("wait for native detail info timeout\n");
		ret = -ETIMEDOUT;
	} else if (ret < 0) {
		memcheck_err("wait for native detail info return %d\n", ret);
		ret = -EFAULT;
	} else {
		memcheck_err("can not get native detail info return %d\n", ret);
		ret = -EFAULT;
	}
	is_waiting = false;
	mutex_unlock(&native_detail_mutex);

	return ret;
}

static int memcheck_send_native_signal(const struct detail_info *info)
{
	int ret = 0;
	struct task_struct *p = NULL;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	kernel_siginfo_t siginfo;

	clear_siginfo(&siginfo);
#else
	struct siginfo siginfo;

	memset(&siginfo, 0, sizeof(siginfo));
#endif

	siginfo.si_signo = SIGNO_MEMCHECK;
	siginfo.si_errno = 0;
	siginfo.si_code = SI_TKILL;
	siginfo.si_pid = task_tgid_vnr(current);
	siginfo.si_uid = from_kuid_munged(current_user_ns(), current_uid());

	rcu_read_lock();
	p = find_task_by_vpid(info->id);
	if (p)
		get_task_struct(p);
	rcu_read_unlock();

	if (p && (task_tgid_vnr(p) == info->id)) {
		if (info->timestamp != nsec_to_clock_t(p->real_start_time)) {
			memcheck_err("pid %d disappear\n", info->id);
			ret = MEMCHECK_PID_INVALID;
			goto err_pid_disappear;
		}

		siginfo.si_addr = (void *)ADDR_NATIVE_DETAIL_INFO;
		ret = do_send_sig_info(SIGNO_MEMCHECK, &siginfo, p, false);
		memcheck_info("send signal to pid %d, ret=%d\n", info->id, ret);
	}

err_pid_disappear:
	if (p)
		put_task_struct(p);
	if (!ret)
		ret = memcheck_wait_native_detail_ready();

	return ret;
}

static int memcheck_native_detail_read(void *buf, struct detail_info *info)
{
	int ret = -EFAULT;
	size_t len;

	mutex_lock(&native_detail_mutex);
	len = min(native_detail_len, info->size - sizeof(*info));
	if (len && copy_to_user(buf + sizeof(*info), native_detail_buf, len)) {
		memcheck_err("copy_to_user failed\n");
		goto unlock;
	}

	if (len != info->size) {
		info->size = len;
		if (copy_to_user(buf, info, sizeof(*info))) {
			memcheck_err("copy_to_user failed\n");
			goto unlock;
		}
	}
	memcheck_info("read native detail success, len=%lu\n", len);
	vfree(native_detail_buf);
	native_detail_buf = NULL;
	native_detail_len = 0;
	ret = 0;
unlock:
	mutex_unlock(&native_detail_mutex);

	return ret;
}

int memcheck_detail_read(void *buf, struct detail_info *info)
{
	char *tmp = NULL;
	size_t num;
	size_t len;
	int ret = -EFAULT;

	if (info->type == MTYPE_USER_PSS_NATIVE) {
		ret = memcheck_send_native_signal(info);
		if (!ret)
			ret = memcheck_native_detail_read(buf, info);
		return ret;
	}

	if (info->type == MTYPE_KERN_SLUB)
		num = info->size / sizeof(struct mm_slub_detail_info);
	else if ((info->type == MTYPE_KERN_ION) ||
		 (info->type == MTYPE_USER_ION) ||
		 (info->type == MTYPE_KERN_ASHMEM) ||
		 (info->type == MTYPE_USER_ASHMEM))
		num = (info->size - EXTEND_SIZE) /
		      sizeof(struct mm_detail_info);
	else if (info->type == MTYPE_KERN_VMALLOC)
		num = info->size / sizeof(struct mm_vmalloc_detail_info);
	else if (info->type == MTYPE_USER_GPU)
		num = (info->size - EXTEND_SIZE) /
		      sizeof(struct mm_detail_info);
	else if (info->type == MTYPE_KERN_GPU)
		num = info->size / sizeof(struct mm_vmalloc_detail_info);
	else {
		memcheck_err("invalid info type: %d\n", info->type);
		return -EINVAL;
		}
	if (!num) {
		info->size = 0;
		if (copy_to_user(buf, info, sizeof(*info))) {
			memcheck_err("copy_to_user failed\n");
			return -EFAULT;
		}

		memcheck_err("buf is too small to contain detail data\n");
		return 0;
	}

	tmp = vzalloc(info->size + 1);
	if (!tmp)
		return -ENOMEM;
	if (info->type == MTYPE_KERN_SLUB)
		len = memcheck_slub_detail_read(num, tmp, info->size);
	else if ((info->type == MTYPE_KERN_ION) ||
		 (info->type == MTYPE_USER_ION))
		len = memcheck_ion_ashmem_detail_read(num, tmp, info->size,
						      ION_TRACK);
	else if ((info->type == MTYPE_KERN_ASHMEM) ||
		 (info->type == MTYPE_USER_ASHMEM))
		len = memcheck_ion_ashmem_detail_read(num, tmp, info->size,
						      ASHMEM_TRACK);
	else if ((info->type == MTYPE_KERN_GPU) ||
		 (info->type == MTYPE_USER_GPU)) {
		len = memcheck_gpu_detail_read(num, tmp, info->size);
		len += gpumem_dir_info(0, tmp + len, info->size - len,
				       info->id);
	} else if (info->type == MTYPE_KERN_VMALLOC)
		len = memcheck_vmalloc_detail_read(num, tmp, info->size);
	else
		goto err_buf;

	info->size = len;
	if (copy_to_user(buf, info, sizeof(*info))) {
		memcheck_err("copy_to_user failed\n");
		goto err_buf;
	}
	if (len && copy_to_user(buf + sizeof(*info), tmp, len)) {
		memcheck_err("copy_to_user failed\n");
		goto err_buf;
	}
	ret = 0;

err_buf:
	vfree(tmp);
	return ret;
}
