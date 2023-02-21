/*
 * hw_graded_sched.h
 *
 * hw graded schedule declaration
 *
 * Copyright (c) 2022-2022 Huawei Technologies Co., Ltd.
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

#ifndef __SCHED_HW_GRADED_SCHED_H__
#define __SCHED_HW_GRADED_SCHED_H__

void set_grade_switch(bool enable);
bool get_grade_switch(void);

void update_graded_nice(struct task_struct *task, bool increase);
void init_graded_nice(struct task_struct *task, bool increase);

#endif
