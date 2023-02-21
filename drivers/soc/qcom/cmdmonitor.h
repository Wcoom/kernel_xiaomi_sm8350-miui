/*
 * cmd_monitor.h
 *
 * cmdmonitor function declaration
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
#ifndef CMD_MONITOR_H
#define CMD_MONITOR_H

#include <linux/version.h>
#include <linux/time.h>

#define S_TO_MS  1000
#define NS_TO_US 1000
#define S_TO_US  1000000
#define NS_TO_MS 1000000

struct time_spec {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
	struct timespec ts;
#else
	struct timespec64 ts;
#endif
};

#define TASK_COMM_LEN 16

struct cmd_monitor {
	struct list_head list;
	struct time_spec sendtime;
	struct time_spec lasttime;
	int32_t timer_index;
	int count;
	bool returned;
	pid_t pid;
	pid_t tid;
	char pname[TASK_COMM_LEN];
	char tname[TASK_COMM_LEN];
	unsigned int lastcmdid;
	long long timetotal;
};

struct cmd_monitor *cmd_monitor_log(uint32_t cmd);
void cmd_monitor_logend(struct cmd_monitor *item);
void init_cmd_monitor(void);

#endif
