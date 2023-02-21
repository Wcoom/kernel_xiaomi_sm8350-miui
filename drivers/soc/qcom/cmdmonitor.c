/*
 * cmd_monitor.c
 *
 * cmdmonitor function, monitor every cmd which is sent to TEE.
 *
 * Copyright (c) 2021-2021 Huawei Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "cmdmonitor.h"
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <securec.h>
#include <linux/sched/task.h>


static LIST_HEAD(g_cmd_monitor_list);
static int g_cmd_monitor_list_size;
#define MAX_CMD_MONITOR_LIST 200
#define TIMETOTALLIMIT (500 * 1000) // calc in us, means 500ms

static DEFINE_MUTEX(g_cmd_monitor_lock);

/* independent wq to avoid block system_wq */
static struct workqueue_struct *g_cmd_monitor_wq;
static struct delayed_work g_cmd_monitor_work;

static void get_time_spec(struct time_spec *time)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
	time->ts = current_kernel_time();
#else
	ktime_get_coarse_ts64(&time->ts);
#endif
}

static void schedule_cmd_monitor_work(struct delayed_work *work,
	unsigned long delay)
{
	if (g_cmd_monitor_wq)
		queue_delayed_work(g_cmd_monitor_wq, work, delay);
	else
		schedule_delayed_work(work, delay);
}

static int get_pid_name(pid_t pid, char *comm, size_t size)
{
	struct task_struct *task = NULL;
	int sret;

	if (size <= TASK_COMM_LEN - 1 || !comm)
		return -1;

	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();
	if (!task) {
		pr_err("get task failed\n");
		return -1;
	}

	sret = strncpy_s(comm, size, task->comm, strlen(task->comm));
	if (sret)
		pr_err("strncpy faild: errno = %d\n", sret);
	put_task_struct(task);

	return sret;
}

const int32_t g_timer_step[] = {1, 1, 1, 2, 5, 10, 40, 120, 180, 360};
const int32_t g_timer_nums = sizeof(g_timer_step) / sizeof(int32_t);
static void show_timeout_cmd_info(struct cmd_monitor *monitor)
{
	long long timedif, timedif2;
	struct time_spec nowtime;
	int32_t time_in_sec = (monitor->timer_index >= g_timer_nums) ?
		g_timer_step[g_timer_nums - 1] : g_timer_step[monitor->timer_index];

	get_time_spec(&nowtime);

	/*
	 * 1 year means 1000 * (60*60*24*365) = 0x757B12C00
	 * only 5bytes, so timedif (timedif=nowtime-sendtime) will not overflow
	 */
	timedif = S_TO_MS * (nowtime.ts.tv_sec - monitor->sendtime.ts.tv_sec) +
		(nowtime.ts.tv_nsec - monitor->sendtime.ts.tv_nsec) / NS_TO_MS;

	timedif2 = S_TO_MS * (nowtime.ts.tv_sec - monitor->lasttime.ts.tv_sec) +
		(nowtime.ts.tv_nsec - monitor->lasttime.ts.tv_nsec) / NS_TO_MS;

	if (timedif2 > (time_in_sec * S_TO_MS)) {
		monitor->lasttime = nowtime;
		monitor->timer_index = (monitor->timer_index >= sizeof(g_timer_step)) ?
			sizeof(g_timer_step) : (monitor->timer_index + 1);
		pr_warn("[cmd_monitor_tick] pid=%d, pname=%s, tid=%d, tname=%s, lastcmdid=%x, "
			"timedif=%lld ms\n", monitor->pid, monitor->pname, monitor->tid,
			monitor->tname, monitor->lastcmdid, timedif);
	}
}

static void cmd_monitor_tick(void)
{
	struct cmd_monitor *monitor = NULL;
	struct cmd_monitor *tmp = NULL;

	mutex_lock(&g_cmd_monitor_lock);
	list_for_each_entry_safe(monitor, tmp, &g_cmd_monitor_list, list) {
		if (monitor->returned) {
			g_cmd_monitor_list_size--;
			if (monitor->timetotal > TIMETOTALLIMIT)
				pr_info("[cmd_monitor_tick] pid=%d, pname=%s, tid=%d, "
					"tname=%s, lastcmdid=%x, count=%d, "
					"timetotal=%lld us returned, remained command(s)=%d\n",
					monitor->pid, monitor->pname, monitor->tid, monitor->tname,
					monitor->lastcmdid, monitor->count, monitor->timetotal,
					g_cmd_monitor_list_size);
			list_del(&monitor->list);
			kfree(monitor);
			continue;
		}
		show_timeout_cmd_info(monitor);
	}

	/* if have cmd in monitor list, we need tick */
	if (g_cmd_monitor_list_size > 0)
		schedule_cmd_monitor_work(&g_cmd_monitor_work, usecs_to_jiffies(S_TO_US));
	mutex_unlock(&g_cmd_monitor_lock);
}

static void cmd_monitor_tickfn(struct work_struct *work)
{
	(void)(work);
	cmd_monitor_tick();
}

static struct cmd_monitor *init_monitor_locked(void)
{
	struct cmd_monitor *newitem = NULL;

	newitem = kzalloc(sizeof(*newitem), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR((unsigned long)(uintptr_t)newitem)) {
		pr_err("[cmd_monitor_tick]kzalloc faild\n");
		return NULL;
	}

	get_time_spec(&newitem->sendtime);
	newitem->lasttime = newitem->sendtime;
	newitem->timer_index = 0;
	newitem->timetotal = 0;
	newitem->count = 1;
	newitem->returned = false;
	newitem->pid = current->tgid;
	newitem->tid = current->pid;
	if (get_pid_name(newitem->pid, newitem->pname,
		sizeof(newitem->pname)))
		newitem->pname[0] = '\0';
	if (get_pid_name(newitem->tid, newitem->tname,
		sizeof(newitem->tname)))
		newitem->tname[0] = '\0';
	INIT_LIST_HEAD(&newitem->list);
	list_add_tail(&newitem->list, &g_cmd_monitor_list);
	g_cmd_monitor_list_size++;
	return newitem;
}

struct cmd_monitor *cmd_monitor_log(uint32_t cmd)
{
	bool found_flag = false;
	pid_t pid;
	pid_t tid;
	struct cmd_monitor *monitor = NULL;

	pid = current->tgid;
	tid = current->pid;
	mutex_lock(&g_cmd_monitor_lock);
	do {
		list_for_each_entry(monitor, &g_cmd_monitor_list, list) {
			if (monitor->pid == pid && monitor->tid == tid) {
				found_flag = true;
				/* restart */
				get_time_spec(&monitor->sendtime);
				monitor->timer_index = 0;
				monitor->count++;
				monitor->returned = false;
				monitor->lastcmdid = cmd;
				monitor->timetotal = 0;
				monitor->lasttime = monitor->sendtime;
				break;
			}
		}

		if (!found_flag) {
			if (g_cmd_monitor_list_size >
				MAX_CMD_MONITOR_LIST - 1) {
				pr_err("monitor reach max node num\n");
				monitor = NULL;
				break;
			}
			monitor = init_monitor_locked();
			if (!monitor) {
				pr_err("init monitor failed\n");
				break;
			}
			monitor->lastcmdid = cmd;
			/* the first cmd will cause timer */
			if (g_cmd_monitor_list_size == 1)
				schedule_cmd_monitor_work(&g_cmd_monitor_work,
					usecs_to_jiffies(S_TO_US));
		}
	} while (0);
	mutex_unlock(&g_cmd_monitor_lock);

	return monitor;
}

void cmd_monitor_logend(struct cmd_monitor *item)
{
	struct time_spec nowtime;
	long long timedif;

	if (!item)
		return;

	get_time_spec(&nowtime);
	/*
	 * get time value D (timedif=nowtime-sendtime),
	 * we do not care about overflow
	 * 1 year means 1000000 * (60*60*24*365) = 0x1CAE8C13E000
	 * only 6bytes, will not overflow
	 */
	timedif = S_TO_US * (nowtime.ts.tv_sec - item->sendtime.ts.tv_sec) +
		(nowtime.ts.tv_nsec - item->sendtime.ts.tv_nsec) / NS_TO_US;
	item->timetotal += timedif;
	item->returned = true;
}

void init_cmd_monitor(void)
{
	g_cmd_monitor_wq = alloc_ordered_workqueue("tz_cmd_monitor_wq", 0);
	if (!g_cmd_monitor_wq)
		pr_err("alloc cmd monitor wq failed\n");

	INIT_DEFERRABLE_WORK((struct delayed_work *)
		(uintptr_t)&g_cmd_monitor_work, cmd_monitor_tickfn);
}
