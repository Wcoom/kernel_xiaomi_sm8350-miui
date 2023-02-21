/*
 * memcheck_memstat.c
 *
 * provide memstat information to user space to determine if leak happens
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

#include "memcheck_memstat.h"
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/signal.h>
#if (KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE)
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#endif
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
#include <linux/signal_types.h>
#include <linux/pagewalk.h>
#endif
#include "memcheck_detail.h"
#include "memcheck_stack.h"

#define PSS_SHIFT	12
#define IDX_JAVA	0
#define IDX_NATIVE	1
#define ADDR_NUM	2

#define JAVA_TAG	"dalvik-"
#define JAVA_TAG_LEN	7
#define JAVA_TAG2	"maple_alloc_ros"
#define JAVA_TAG2_LEN	15

#define NATIVE_TAG2	"libc_malloc"
#define NATIVE_TAG2_LEN	11

#define MAX_TOP_NUM 10
#define TOPN_BUF_LEN 4
#define DEFAULT_TOPN 3

/* for lmk and oom record save and read */
static DEFINE_SPINLOCK(list_lock);
static struct lmk_oom_rec *rec_list;
static u64 rec_num;
static int cur_topn = DEFAULT_TOPN;

struct memsize_stats {
	unsigned long resident;
	unsigned long swap;
	u64 pss;
	u64 native_rss;
	u64 java_rss;
	u64 vss;
};

enum heap_type {
	HEAP_OTHER,
	HEAP_JAVA,
	HEAP_NATIVE,
};

static char top_slub_name[SLUB_NAME_LEN];

static const char *const java_tag[] = {
	"dalvik-alloc space",
	"dalvik-main space",
	"dalvik-large object space",
	"dalvik-free list large object space",
	"dalvik-non moving space",
	"dalvik-zygote space",
};

static u64 addr_array[][ADDR_NUM] = {
	/* MEMCMD_NONE */
	{ 0, 0 },
	/* MEMCMD_ENABLE */
	{ ADDR_JAVA_ENABLE, ADDR_NATIVE_ENABLE },
	/* MEMCMD_DISABLE */
	{ ADDR_JAVA_DISABLE, ADDR_NATIVE_DISABLE },
	/* MEMCMD_SAVE_LOG */
	{ ADDR_JAVA_SAVE, ADDR_NATIVE_SAVE },
	/* MEMCMD_CLEAR_LOG */
	{ ADDR_JAVA_CLEAR, ADDR_NATIVE_CLEAR },
};

static bool is_java_heap(const char *tag)
{
	int i;
	char *tmp = NULL;

	if (strncmp(tag, JAVA_TAG2, JAVA_TAG2_LEN) == 0)
		return true;

	for (i = 0; i < ARRAY_SIZE(java_tag); i++) {
		tmp = strstr(tag, java_tag[i]);
		if (tmp == tag)
			return true;
	}

	return false;
}

static bool is_native_heap(const char *tag)
{
	if (strncmp(tag, NATIVE_TAG2, NATIVE_TAG2_LEN) == 0)
		return true;
	return false;
}

static enum heap_type memcheck_get_heap_type(const char *name)
{
	enum heap_type type = HEAP_OTHER;

	if (!name)
		return type;

	if (is_native_heap(name))
		type = HEAP_NATIVE;
	else if (is_java_heap(name))
		type = HEAP_JAVA;
	return type;
}

static struct page **alloc_page_pointers(size_t num)
{
	struct page **page = NULL;
	size_t page_len = sizeof(**page) * num;

	page = kzalloc(page_len, GFP_KERNEL);
	if (!page)
		return ERR_PTR(-ENOMEM);

	return page;
}

static size_t do_strncpy_from_remote_string(char *dst, long page_offset,
					    struct page **page, long num_pin,
					    long count)
{
	long i;
	size_t sz;
	size_t strsz;
	size_t copy_sum = 0;
	long page_left = min((long)PAGE_SIZE - page_offset, count);
	const char *p = NULL;
	const char *kaddr = NULL;

	count = min(count, num_pin * (long)PAGE_SIZE - page_offset);

	for (i = 0; i < num_pin; i++) {
		kaddr = (const char *)kmap(page[i]);
		if (!kaddr)
			break;

		if (i == 0) {
			p = kaddr + page_offset;
			sz = page_left;
		} else {
			p = kaddr;
			sz = min((long)PAGE_SIZE, count - page_left -
				 (i - 1) * (long)PAGE_SIZE);
		}

		strsz = strnlen(p, sz);
		memcpy(dst, p, strsz);

		kunmap(page[i]);

		dst += strsz;
		copy_sum += strsz;

		if (strsz != sz)
			break;
	}

	for (i = 0; i < num_pin; i++)
		put_page(page[i]);

	return copy_sum;
}

static long strncpy_from_remote_user(char *dst, struct mm_struct *remote_mm,
				    const char __user *src, long count)
{
	long num_pin;
	size_t copy_sum;
	struct page **page = NULL;

	uintptr_t src_page_start = (uintptr_t)src & PAGE_MASK;
	uintptr_t src_page_offset = (uintptr_t)(src - src_page_start);
	size_t num_pages = DIV_ROUND_UP(src_page_offset + count,
					(long)PAGE_SIZE);

	page = alloc_page_pointers(num_pages);
	if (IS_ERR_OR_NULL(page))
		return PTR_ERR(page);

#if (KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE)
	num_pin = get_user_pages_remote(current, remote_mm,
					src_page_start, num_pages, 0,
					page, NULL, NULL);
#else
	num_pin = get_user_pages_remote(current, remote_mm,
					src_page_start, num_pages, 0,
					page, NULL);
#endif
	if (num_pin < 1) {
		kfree(page);
		return 0;
	}

	copy_sum = do_strncpy_from_remote_string(dst, src_page_offset, page,
						 num_pin, count);
	kfree(page);

	return copy_sum;
}

static enum heap_type memcheck_anon_vma_name(struct vm_area_struct *vma)
{
	const char __user *name_user = vma_get_anon_name(vma);
	unsigned long max_len = min((unsigned long)NAME_MAX + 1,
				    (unsigned long)PAGE_SIZE);
	char *out_name = NULL;
	enum heap_type type = HEAP_OTHER;
	long retcpy;

	out_name = kzalloc(max_len, GFP_KERNEL);
	if (!out_name)
		return type;

	retcpy = strncpy_from_remote_user(out_name, vma->vm_mm,
					  name_user, max_len);
	if (retcpy <= 0)
		goto free_name;

	type = memcheck_get_heap_type(out_name);

free_name:
	kfree(out_name);

	return type;
}

enum heap_type memcheck_get_type(struct vm_area_struct *vma)
{
	char *name = NULL;
	struct mm_struct *mm = vma->vm_mm;
	enum heap_type type = HEAP_OTHER;

	/* file map is never heap in Android Q */
	if (vma->vm_file)
		return type;

	/* get rid of stack */
	if ((vma->vm_start <= vma->vm_mm->start_stack) &&
	    (vma->vm_end >= vma->vm_mm->start_stack))
		return type;

	if ((vma->vm_ops) && (vma->vm_ops->name)) {
		name = (char *)vma->vm_ops->name(vma);
		if (name)
			goto got_name;
	}

	name = (char *)arch_vma_name(vma);
	if (name)
		goto got_name;

	/* get rid of vdso */
	if (!mm)
		return type;

	/* main thread native heap */
	if ((vma->vm_start <= mm->brk) && (vma->vm_end >= mm->start_brk))
		return HEAP_NATIVE;

	if (vma_get_anon_name(vma))
		return memcheck_anon_vma_name(vma);

got_name:
	return memcheck_get_heap_type(name);
}

static void memcheck_pte_entry(pte_t *pte, unsigned long addr,
				    struct mm_walk *walk)
{
	struct memsize_stats *mss = walk->private;
	struct vm_area_struct *vma = walk->vma;
	struct page *page = NULL;

	if (pte_present(*pte)) {
		page = vm_normal_page(vma, addr, *pte);
	} else if (is_swap_pte(*pte)) {
		swp_entry_t swpent = pte_to_swp_entry(*pte);

		if (!non_swap_entry(swpent))
			mss->swap += PAGE_SIZE;
		else if (is_migration_entry(swpent))
			page = migration_entry_to_page(swpent);
	}
	if (page)
		mss->resident += PAGE_SIZE;
}

static int memcheck_pte_range(pmd_t *pmd, unsigned long addr,
			      unsigned long end, struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;
	pte_t *pte = NULL;
	spinlock_t *ptl = NULL;

	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; pte++, addr += PAGE_SIZE)
		memcheck_pte_entry(pte, addr, walk);
	pte_unmap_unlock(pte - 1, ptl);
	cond_resched();

	return 0;
}

static void add_swap_to_rss(struct vm_area_struct *vma,
			    struct memsize_stats *mss,
			    struct memsize_stats *mss_total)
{
	enum heap_type type;

	mss->resident += mss->swap;
	mss_total->resident += mss->resident;
	mss_total->vss += vma->vm_end - vma->vm_start;

	type = memcheck_get_type(vma);
	if (type == HEAP_JAVA)
		mss_total->java_rss += mss->resident;
	else if (type == HEAP_NATIVE)
		mss_total->native_rss += mss->resident;
}

static int memcheck_get_mss(pid_t pid, struct memsize_stats *mss_total)
{
	struct task_struct *tsk = NULL;
	struct mm_struct *mm = NULL;
	struct vm_area_struct *vma = NULL;
	struct memsize_stats mss;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	struct mm_walk_ops ops = {
		.pmd_entry = memcheck_pte_range,
	};
#else
	struct mm_walk memstat_walk = {
		.pmd_entry = memcheck_pte_range,
		.private = &mss,
	};
#endif

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();
	if (!tsk)
		return -EINVAL;

	mm = get_task_mm(tsk);
	if (!mm) {
		put_task_struct(tsk);
		return -EINVAL;
	}

	memset(mss_total, 0, sizeof(*mss_total));

	down_read(&mm->mmap_sem);
	vma = mm->mmap;
	while (vma) {
		memset(&mss, 0, sizeof(mss));
		if ((vma->vm_mm) && (!is_vm_hugetlb_page(vma))) {
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
			walk_page_range(vma->vm_mm, vma->vm_start, vma->vm_end,
					&ops, &mss);
#else
			memstat_walk.mm = vma->vm_mm;
			walk_page_range(vma->vm_start, vma->vm_end,
					&memstat_walk);
#endif
			/* simply add swap to resident to get a total rss */
			add_swap_to_rss(vma, &mss, mss_total);
		}
		vma = vma->vm_next;
	}
	up_read(&mm->mmap_sem);
	mmput(mm);
	put_task_struct(tsk);
	return 0;
}

unsigned short memcheck_get_memstat(struct memstat_all *p)
{
	int ret;
	unsigned int i;
	struct memsize_stats mss_total;
	unsigned short result = 0;

	/* get userspace memstat */
	if ((p->type & MTYPE_USER_PSS) || (p->type & MTYPE_USER_VSS)) {
		memset(&mss_total, 0, sizeof(mss_total));

		/* read the smaps */
		ret = memcheck_get_mss(p->id, &mss_total);
		if (!ret) {
			if (p->type & MTYPE_USER_PSS_JAVA) {
				p->pss.java_pss = mss_total.java_rss;
				result = result | MTYPE_USER_PSS_JAVA;
			}
			if (p->type & MTYPE_USER_PSS_NATIVE) {
				p->pss.native_pss = mss_total.native_rss;
				result = result | MTYPE_USER_PSS_NATIVE;
			}
			if (p->type & MTYPE_USER_PSS)
				p->total_pss = mss_total.resident;
			if (p->type & MTYPE_USER_VSS) {
				p->vss = mss_total.vss;
				result = result | MTYPE_USER_VSS;
			}
		}
	}

	if (p->type & MTYPE_USER_ION) {
		p->ion_pid = mm_get_ion_by_pid(p->id);
		if (p->ion_pid)
			result = result | MTYPE_USER_ION;
	}
	if (p->type & MTYPE_USER_ASHMEM) {
		p->ashmem_pid = mm_get_ashmem_by_pid(p->id);
		if (p->ashmem_pid)
			result = result | MTYPE_USER_ASHMEM;
	}
	if (p->type & MTYPE_USER_GPU) {
		p->gpu_pid = mm_get_gpu_by_pid(p->id);
		if (p->gpu_pid)
			result = result | MTYPE_USER_GPU;
	}

	if (!(p->type & MTYPE_KERNEL))
		return result;

	/* get kernel memstat */
	for (i = 0; i < NUM_KERN_MAX; i++) {
		if (memcheck_bit_shift(p->type, i + IDX_KERN_START)) {
			p->memory = mm_get_mem_total(i);
			if (p->memory) {
				result |= (1 << (i + IDX_KERN_START));
				break;
			}
		}
	}

	return result;
}

static bool process_disappear(u64 t, const struct track_cmd *cmd)
{
	if (cmd->cmd == MEMCMD_ENABLE)
		return false;
	if (cmd->timestamp != nsec_to_clock_t(t))
		return true;

	return false;
}

void memcheck_save_top_slub(const char *name)
{
	memcpy(top_slub_name, name, sizeof(top_slub_name));
}

int memcheck_do_kernel_command(const struct track_cmd *cmd)
{
	int ret = 0;

	switch (cmd->cmd) {
	case MEMCMD_ENABLE:
		if (cmd->type == MTYPE_KERN_SLUB) {
			memcheck_slub_detail_preread();
			memcheck_info("top1 slub is %s\n", top_slub_name);
			ret = mm_page_trace_on(SLUB_TRACK, top_slub_name);
		} else if (cmd->type == MTYPE_KERN_BUDDY) {
			ret = mm_page_trace_on(BUDDY_TRACK, "buddy");
		} else if (cmd->type == MTYPE_KERN_LSLUB) {
			ret = mm_page_trace_on(LSLUB_TRACK, "lsub");
		}
		if (ret)
			memcheck_err("trace on failed, memtype=%d\n",
				     cmd->type);
		else
			memcheck_info("trace on success, memtype=%d\n",
				      cmd->type);
		break;
	case MEMCMD_DISABLE:
		if (cmd->type == MTYPE_KERN_SLUB)
			ret = mm_page_trace_off(SLUB_TRACK, top_slub_name);
		else if (cmd->type == MTYPE_KERN_BUDDY)
			ret = mm_page_trace_off(BUDDY_TRACK, "buddy");
		else if (cmd->type == MTYPE_KERN_LSLUB)
			ret = mm_page_trace_off(LSLUB_TRACK, "lsub");
		if (ret)
			memcheck_err("trace off failed, memtype=%d\n",
				     cmd->type);
		else
			memcheck_info("trace off success, memtype=%d\n",
				      cmd->type);
		break;
	case MEMCMD_SAVE_LOG:
		break;
	case MEMCMD_CLEAR_LOG:
		break;
	default:
		break;
	}

	return ret;
}

int memcheck_do_command(const struct track_cmd *cmd)
{
	int ret = 0;
	struct task_struct *p = NULL;
	u64 addr = 0;
	bool is_java = (cmd->type & MTYPE_USER_PSS_JAVA) ? true : false;
	bool is_native = (cmd->type & MTYPE_USER_PSS_NATIVE) ? true : false;
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	kernel_siginfo_t info;

	clear_siginfo(&info);
#else
	struct siginfo info;

	memset(&info, 0, sizeof(info));
#endif

	if (cmd->type & MTYPE_KERNEL)
		return memcheck_do_kernel_command(cmd);

	if (is_java == is_native) {
		memcheck_err("invalid type=%d\n", cmd->type);
		return -EFAULT;
	}
	info.si_signo = SIGNO_MEMCHECK;
	info.si_errno = 0;
	info.si_code = SI_TKILL;
	info.si_pid = task_tgid_vnr(current);
	info.si_uid = from_kuid_munged(current_user_ns(), current_uid());

	rcu_read_lock();
	p = find_task_by_vpid(cmd->id);
	if (p)
		get_task_struct(p);
	rcu_read_unlock();

	if (p && (task_tgid_vnr(p) == cmd->id)) {
		if (process_disappear(p->real_start_time, cmd)) {
			memcheck_err("pid %d disappear\n", cmd->id);
			ret = MEMCHECK_PID_INVALID;
			goto err_pid_disappear;
		}

		if (is_java)
			addr = addr_array[cmd->cmd][IDX_JAVA];
		if (is_native)
			addr |= addr_array[cmd->cmd][IDX_NATIVE];
		info.si_addr = (void *)addr;
		if (is_java || is_native)
			ret = do_send_sig_info(SIGNO_MEMCHECK, &info, p, false);
	}

err_pid_disappear:
	if (p)
		put_task_struct(p);
	if ((!ret) && (cmd->cmd == MEMCMD_SAVE_LOG))
		memcheck_wait_stack_ready(cmd->type);
	else if ((!ret) && (cmd->cmd == MEMCMD_CLEAR_LOG))
		memcheck_stack_clear();

	return ret;
}

int memcheck_get_task_type(void *buf, struct task_type_read *read)
{
	size_t num = 0;
	int ret = -EFAULT;
	struct task_type_rec *list = NULL;
	struct task_struct *p = NULL;

	list = vzalloc(read->num * sizeof(*list));
	if (!list)
		return 0;

	rcu_read_lock();
	for_each_process(p) {
		if (p->flags & PF_KTHREAD)
			continue;
		if (p->pid != p->tgid)
			continue;

		list[num].pid = p->pid;
		list[num].is_32bit =
		    test_ti_thread_flag(task_thread_info(p), TIF_32BIT);
		num++;
		if (num >= read->num)
			break;
	}
	rcu_read_unlock();

	/* num = 0 is valid return */
	read->num = num;
	if (copy_to_user(buf, read, sizeof(*read))) {
		memcheck_err("copy task type record num failed\n");
		goto err_buf;
	}
	if (!num) {
		ret = 0;
		goto err_buf;
	}
	if (copy_to_user(buf + sizeof(*read), list,
			 num * sizeof(read->data[0]))) {
		memcheck_err("copy task type record failed\n");
		goto err_buf;
	}
	ret = 0;

err_buf:
	vfree(list);

	return ret;
}

int memcheck_lmk_oom_read(void *buf, struct lmk_oom_read *read)
{
	size_t num;
	int ret = -EFAULT;
	struct lmk_oom_rec list_temp[MEMCHECK_OOM_LMK_MAXNUM];
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	/* num = 0 is valid return */
	memset(&list_temp[0], 0, sizeof(list_temp));
	num = min(rec_num, read->num);
	read->num = num;
	if (!num || !rec_list)
		goto copy_head;
	memcpy(&list_temp[0], &rec_list[0], num * sizeof(read->data[0]));
copy_head:
	spin_unlock_irqrestore(&list_lock, flags);

	if (copy_to_user(buf, read, sizeof(*read))) {
		memcheck_err("copy oom record num failed\n");
		return ret;
	}
	if (num && copy_to_user(buf + sizeof(*read), list_temp,
				num * sizeof(read->data[0]))) {
		memcheck_err("copy oom record failed\n");
		return ret;
	}

	ret = 0;
	return ret;
}

int memcheck_lmk_oom_write(const struct lmk_oom_write *write)
{
	size_t len;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	if (!rec_list) {
		rec_list = kcalloc(MEMCHECK_OOM_LMK_MAXNUM,
				   sizeof(write->data), GFP_ATOMIC);
		if (!rec_list) {
			spin_unlock_irqrestore(&list_lock, flags);
			return -ENOMEM;
		}
	}
	if (rec_num == MEMCHECK_OOM_LMK_MAXNUM) {
		len = (MEMCHECK_OOM_LMK_MAXNUM - 1) * sizeof(rec_list[0]);
		memmove(&rec_list[0], &rec_list[1], len);
		len = sizeof(write->data);
		memcpy(&rec_list[MEMCHECK_OOM_LMK_MAXNUM - 1],
		       &write->data, len);
	} else {
		len = sizeof(write->data);
		memcpy(&rec_list[rec_num], &write->data, len);
		rec_num++;
	}

	spin_unlock_irqrestore(&list_lock, flags);

	return 0;
}

int memcheck_report_lmk_oom(pid_t pid, pid_t tgid, const char *name,
			    enum kill_type ktype, short adj, size_t pss)
{
	struct lmk_oom_write wr;
	size_t len;

	memset(&wr, 0, sizeof(wr));
	wr.magic = MEMCHECK_MAGIC;
	wr.data.pid = pid;
	wr.data.tgid = tgid;
	if (name) {
		len = strnlen(name, sizeof(wr.data.name) - 1);
		if (len)
			strncpy(wr.data.name, name, len);
	}
	wr.data.ktype = ktype;
	wr.data.adj = adj;
	wr.data.pss = pss;
	wr.data.timestamp = jiffies_to_msecs(jiffies);

	return memcheck_lmk_oom_write(&wr);
}
EXPORT_SYMBOL(memcheck_report_lmk_oom);

static int rss_topn_info_show(struct seq_file *s, void *ptr)
{
	int i;
	struct task_struct *tsk = NULL;
	unsigned long rss[MAX_TOP_NUM] = { 0 };
	int tgid[MAX_TOP_NUM] = { 0 };
	char name[MAX_TOP_NUM][TASK_COMM_LEN] = { 0 };

	seq_printf(s, "%16s %6s %16s\n", "task", "tgid", "rss");

	rcu_read_lock();
	for_each_process(tsk) {
		int index = 0;
		unsigned long cur_rss;
		struct mm_struct *mm = NULL;

		if (tsk->flags & PF_KTHREAD)
			continue;
		mm = get_task_mm(tsk);
		if (!mm)
			continue;
		cur_rss = get_mm_rss(mm) + get_mm_counter(mm, MM_SWAPENTS);
		for (i = 0; i < cur_topn; i++) {
			if (cur_rss > rss[i]) {
				index = i;
				break;
			}
		}
		if (i >= cur_topn) {
			mmput(mm);
			continue;
		}
		rss[index] = cur_rss;
		tgid[index] = tsk->tgid;
		memcpy(name[index], tsk->comm, TASK_COMM_LEN);
		mmput(mm);
	}
	rcu_read_unlock();
	for (i = 0; i < cur_topn; i++)
		seq_printf(s, "%16s %6d %16lu\n", name[i], tgid[i],
			   rss[i] * PAGE_SIZE);

	return 0;
}

static ssize_t rss_topn_info_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	char buffer[TOPN_BUF_LEN];
	int topn;
	int err = 0;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count)) {
		err = -EFAULT;
		goto err;
	}
	err = kstrtoint(strstrip(buffer), 0, &topn);
	if (err)
		goto err;

	if (topn <= 0 || topn > MAX_TOP_NUM) {
		err = -EINVAL;
		goto err;
	}
	cur_topn = topn;

err:
	return err < 0 ? err : count;
}

static int rss_topn_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, rss_topn_info_show, PDE_DATA(inode));
}

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
static const struct proc_ops rss_topn_info_fops = {
	.proc_open = rss_topn_info_open,
	.proc_read = seq_read,
	.proc_write = rss_topn_info_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rss_topn_info_fops = {
	.open = rss_topn_info_open,
	.read = seq_read,
	.write = rss_topn_info_write,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int memcheck_createfs(void)
{
	proc_create_data("rss_topn", 0660, NULL,
			 &rss_topn_info_fops, NULL);
	return 0;
}
