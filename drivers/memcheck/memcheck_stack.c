/*
 * memcheck_stack.c
 *
 * save and read stack information
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

#include "memcheck_stack.h"
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

static int slub_type[] = { SLUB_ALLOC, SLUB_FREE };

static const char *const slub_text[] = {
	"SLUB_ALLOC",
	"SLUB_FREE",
};

/* for stack information save and read */
#define STACK_WAIT_TIME_SEC	5
#define IDX_JAVA		0
#define IDX_NATIVE		1
#define STACK_NUM		2
static DEFINE_MUTEX(stack_mutex);
static void *stack_buf[STACK_NUM];
static u64 stack_len[STACK_NUM];
static wait_queue_head_t stack_ready;
static bool is_waiting[STACK_NUM];

static size_t memcheck_append_str(const char *str, char *buf, size_t total,
				  size_t used)
{
	int tmp;

	tmp = snprintf(buf + used, total - used, "%s\n", str);
	if (tmp < 0)
		return used;
	used += min((size_t) tmp, total - used - 1);
	return used;
}

static int stack_cmp(const void *a, const void *b)
{
	const struct mm_stack_info *info1 = a;
	const struct mm_stack_info *info2 = b;

	if (atomic_read(&info1->ref) < atomic_read(&info2->ref))
		return 1;
	else if (atomic_read(&info1->ref) > atomic_read(&info2->ref))
		return -1;
	else
		return 0;
}

static size_t memcheck_stack_to_str(struct mm_stack_info *list, size_t num,
				    char *buf, size_t total, size_t used)
{
	int i;
	int tmp;

	sort(list, num, sizeof(*list), stack_cmp, NULL);

	tmp = snprintf(buf + used, total - used, "PC	hits\n");
	if (tmp < 0)
		goto buf_done;
	used += min((size_t) tmp, total - used - 1);
	for (i = 0; i < num; i++) {
		tmp = snprintf(buf + used, total - used, "%pS %zu\n",
			       list[i].caller, list[i].ref);
		if (tmp < 0)
			goto buf_done;
		used += min((size_t)tmp, total - used - 1);
		if (used >= (total - 1))
			break;
	}
buf_done:
	return used;
}

static size_t memcheck_get_slub_stack(struct mm_stack_info *list, size_t num,
				      char *buf, size_t total)
{
	int i;
	int ret;
	size_t used = 0;
	size_t ret_num;

	for (i = 0; i < ARRAY_SIZE(slub_type); i++) {
		ret = mm_page_trace_open(SLUB_TRACK, slub_type[i]);
		if (ret) {
			memcheck_info("open SLUB trace failed");
			return used;
		}
		ret_num = mm_page_trace_read(SLUB_TRACK, list, num,
					       slub_type[i]);
		if (ret_num == 0) {
			memcheck_info("empty %s stack record", slub_text[i]);
			mm_page_trace_close(SLUB_TRACK, slub_type[i]);
			continue;
		}
		ret = mm_page_trace_close(SLUB_TRACK, slub_type[i]);
		if (ret) {
			memcheck_info("close SLUB trace failed");
			return used;
		}
		used = memcheck_append_str(slub_text[i], buf, total, used);
		if (used >= (total - 1))
			return used;
		used = memcheck_stack_to_str(list, ret_num, buf, total, used);
		if ((ret_num >= num) || (used >= (total - 1)))
			return used;
	}

	return used;
}

static size_t memcheck_get_vmalloc_stack(struct mm_stack_info *list, size_t num,
					 char *buf, size_t total)
{
	int i;
	int ret;
	size_t used = 0;
	size_t ret_num;

	for (i = 0; i < ARRAY_SIZE(vmalloc_type); i++) {
		ret = mm_page_trace_open(VMALLOC_TRACK, vmalloc_type[i]);
		if (ret) {
			memcheck_info("open VMALLOC trace failed");
			return used;
		}
		ret_num = mm_page_trace_read(VMALLOC_TRACK, list, num,
					     vmalloc_type[i]);
		if (ret_num == 0) {
			memcheck_info("empty %s stack record",
				      vmalloc_text[i]);
			mm_page_trace_close(VMALLOC_TRACK, vmalloc_type[i]);
			continue;
		}
		ret = mm_page_trace_close(VMALLOC_TRACK, vmalloc_type[i]);
		if (ret) {
			memcheck_info("close VMALLOC trace failed");
			return used;
		}
		used = memcheck_append_str(vmalloc_text[i], buf, total, used);
		if (used >= (total - 1))
			return used;
		used = memcheck_stack_to_str(list, ret_num, buf, total, used);
		if ((ret_num >= num) || (used >= (total - 1)))
			return used;
	}

	return used;
}

static size_t memcheck_get_buddy_stack(struct mm_stack_info *list, size_t num,
				       char *buf, size_t total)
{
	int ret;
	size_t used = 0;
	size_t ret_num;

	ret = mm_page_trace_open(BUDDY_TRACK, BUDDY_TRACK);
	if (ret) {
		memcheck_info("open BUDDY trace failed");
		return used;
	}
	ret_num = mm_page_trace_read(BUDDY_TRACK, list, num, BUDDY_TRACK);
	if (ret_num == 0) {
		memcheck_info("empty buddy stack record");
		mm_page_trace_close(BUDDY_TRACK, BUDDY_TRACK);
		return used;
	}
	ret = mm_page_trace_close(BUDDY_TRACK, BUDDY_TRACK);
	if (ret) {
		memcheck_info("close BUDDY trace failed");
		return used;
	}
	used = memcheck_stack_to_str(list, ret_num, buf, total, used);
	return used;
}

static size_t memcheck_get_lslub_stack(struct mm_stack_info *list, size_t num,
				       char *buf, size_t total)
{
	int ret;
	size_t used = 0;
	size_t ret_num;

	ret = mm_page_trace_open(LSLUB_TRACK, LSLUB_TRACK);
	if (ret) {
		memcheck_info("open LSLUB trace failed");
		return used;
	}
	ret_num = mm_page_trace_read(LSLUB_TRACK, list, num, LSLUB_TRACK);
	if (ret_num == 0) {
		memcheck_info("empty buddy stack record");
		mm_page_trace_close(LSLUB_TRACK, LSLUB_TRACK);
		return used;
	}
	ret = mm_page_trace_close(LSLUB_TRACK, LSLUB_TRACK);
	if (ret) {
		memcheck_info("close LSLUB trace failed");
		return used;
	}
	used = memcheck_stack_to_str(list, ret_num, buf, total, used);
	return used;
}

static int memcheck_get_stack_items(size_t num, char *buf, size_t total,
				    struct stack_info *info)
{
	struct mm_stack_info *list = NULL;
	size_t used = 0;

	list = vzalloc(num * sizeof(*list));
	if (!list)
		return 0;
	if (info->type == MTYPE_KERN_SLUB)
		used = memcheck_get_slub_stack(list, num, buf, total);
	else if (info->type == MTYPE_KERN_VMALLOC)
		used = memcheck_get_vmalloc_stack(list, num, buf, total);
	else if (info->type == MTYPE_KERN_BUDDY)
		used = memcheck_get_buddy_stack(list, num, buf, total);
	else if (info->type == MTYPE_KERN_LSLUB)
		used = memcheck_get_lslub_stack(list, num, buf, total);
	vfree(list);

	return used;
}

static int memcheck_kernel_stack_read(void *buf, struct stack_info *info)
{
	char *tmp = NULL;
	size_t num;
	size_t len;
	int ret;

	num = info->size / sizeof(struct mm_stack_info);
	if (!num) {
		info->size = 0;
		if (copy_to_user(buf, info, sizeof(*info))) {
			memcheck_err("copy_to_user failed\n");
			return -EFAULT;
		}
		memcheck_info("buf is too small to contain stack data\n");
		return 0;
	}

	tmp = vzalloc(info->size + 1);
	if (!tmp)
		return -ENOMEM;
	len = memcheck_get_stack_items(num, tmp, info->size, info);

	info->size = len;
	if (copy_to_user(buf, info, sizeof(*info))) {
		memcheck_err("copy_to_user failed\n");
		ret = -EFAULT;
		goto err_buf;
	}
	if (len && copy_to_user(buf + sizeof(*info), tmp, len)) {
		memcheck_err("copy_to_user failed\n");
		ret = -EFAULT;
		goto err_buf;
	}
	ret = 0;

err_buf:
	vfree(tmp);
	return ret;
}

int memcheck_stack_read(void *buf, struct stack_info *info)
{
	int ret = -EFAULT;
	size_t len;
	size_t java_len = 0;
	size_t total_len = 0;
	int idx;

	if (info->type & MTYPE_KERNEL)
		return memcheck_kernel_stack_read(buf, info);

	mutex_lock(&stack_mutex);
	for (idx = 0; idx < STACK_NUM; idx++) {
		if (!stack_buf[idx])
			continue;
		len = min(stack_len[idx], info->size - total_len);
		if (copy_to_user
		    (buf + sizeof(*info) + java_len, stack_buf[idx], len)) {
			memcheck_err("copy_to_user failed\n");
			goto unlock;
		}
		if (info->type & MTYPE_USER_PSS_JAVA)
			java_len = len;
		memcheck_info("read idx=%d,len=%llu\n", idx, len);
		total_len += len;
		if (total_len >= info->size)
			break;
	}
	if (total_len != info->size) {
		info->size = total_len;
		if (copy_to_user(buf, info, sizeof(*info))) {
			memcheck_err("copy_to_user failed\n");
			goto unlock;
		}
	}

	ret = 0;

unlock:
	mutex_unlock(&stack_mutex);

	return ret;
}

int memcheck_stack_clear(void)
{
	int idx;

	mutex_lock(&stack_mutex);
	for (idx = 0; idx < STACK_NUM; idx++) {
		if (stack_buf[idx]) {
			vfree(stack_buf[idx]);
			stack_buf[idx] = NULL;
			stack_len[idx] = 0;
		}
	}
	mutex_unlock(&stack_mutex);

	return 0;
}

int memcheck_stack_write(const void *buf, const struct stack_info *info)
{
	char *tmp = NULL;
	int idx;

	tmp = vzalloc(info->size + 1);
	if (!tmp)
		return -EFAULT;
	if (copy_from_user(tmp, buf + sizeof(*info), info->size)) {
		vfree(tmp);
		memcheck_err("copy_from_user failed\n");
		return -EFAULT;
	}
	tmp[info->size] = 0;

	idx = (info->type & MTYPE_USER_PSS_JAVA) ? IDX_JAVA : IDX_NATIVE;
	mutex_lock(&stack_mutex);
	if (stack_buf[idx])
		vfree(stack_buf[idx]);
	stack_buf[idx] = tmp;
	stack_len[idx] = info->size;
	mutex_unlock(&stack_mutex);

	if (is_waiting[idx])
		wake_up_interruptible(&stack_ready);

	return 0;
}

static int memcheck_check_wait_result(int left, bool is_java, bool is_native)
{
	if (!left) {
		if (is_java && (!is_native))
			memcheck_err("wait for java stack timeout\n");
		else if ((!is_java) && is_native)
			memcheck_err("wait for native stack timeout\n");
		else if (is_java && is_native)
			memcheck_err("wait for java and native timeout\n");
		return -ETIMEDOUT;
	} else if (left < 0) {
		if (is_java && (!is_native))
			memcheck_err("wait for java stack return %d\n", left);
		else if ((!is_java) && is_native)
			memcheck_err("wait for native stack return %d\n",
				     left);
		else if (is_java && is_native)
			memcheck_err("wait for java and native return %d\n",
				     left);
		return -EFAULT;
	}

	return 0;
}

int memcheck_wait_stack_ready(u16 type)
{
	int left;
	bool is_java = (type & MTYPE_USER_PSS_JAVA) ? true : false;
	bool is_native = (type & MTYPE_USER_PSS_NATIVE) ? true : false;
	int index;
	int ret = 0;

	index = is_java ? IDX_JAVA : IDX_NATIVE;

	mutex_lock(&stack_mutex);
	is_waiting[index] = true;
	init_waitqueue_head(&stack_ready);
	mutex_unlock(&stack_mutex);
	left = wait_event_interruptible_timeout(stack_ready,
					     stack_buf[index],
					     STACK_WAIT_TIME_SEC * HZ);
	mutex_lock(&stack_mutex);
	if (stack_buf[index])
		memcheck_info("get %s stack successfully\n",
			      is_java ? "java" : "native");
	else
		ret = memcheck_check_wait_result(left, is_java, is_native);
	is_waiting[index] = false;
	mutex_unlock(&stack_mutex);

	return ret;
}
