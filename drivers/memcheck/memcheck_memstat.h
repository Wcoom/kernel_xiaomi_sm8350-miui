/*
 * memcheck_memstat.h
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

#ifndef _MEMCHECK_MEMSTAT_H
#define _MEMCHECK_MEMSTAT_H

#include <linux/types.h>
#include "memcheck_ioctl.h"
#include "impl/memstat_imp.h"

#define SIGNO_MEMCHECK			44
#define ADDR_JAVA_ENABLE		(1 << 0)
#define ADDR_JAVA_DISABLE		(1 << 1)
#define ADDR_JAVA_SAVE			(1 << 2)
#define ADDR_JAVA_CLEAR			(1 << 3)
#define ADDR_NATIVE_ENABLE		(1 << 4)
#define ADDR_NATIVE_DISABLE		(1 << 5)
#define ADDR_NATIVE_SAVE		(1 << 6)
#define ADDR_NATIVE_CLEAR		(1 << 7)
#define ADDR_NATIVE_DETAIL_INFO		(1 << 8)

#ifdef CONFIG_DFX_MEMCHECK
int memcheck_do_command(const struct track_cmd *cmd);
unsigned short memcheck_get_memstat(struct memstat_all *p);
void memcheck_save_top_slub(const char *name);
int memcheck_get_task_type(void *buf, struct task_type_read *read);
int memcheck_lmk_oom_read(void *buf, struct lmk_oom_read *rec);
int memcheck_lmk_oom_write(const struct lmk_oom_write *write);
int memcheck_report_lmk_oom(pid_t pid, pid_t tgid, const char *name,
			    enum kill_type ktype, short adj, size_t pss);
int memcheck_createfs(void);
#else /* CONFIG_DFX_MEMCHECK */
static inline unsigned short memcheck_get_memstat(struct memstat_all *p)
{
	return 0;
}
static inline int memcheck_do_command(struct memstat_all *p)
{
	return 0;
}
static inline void memcheck_save_top_slub(const char *name)
{
}
static inline int memcheck_get_task_type(void *buf, struct task_type_read *read)
{
	return -EINVAL;
}
static inline int memcheck_lmk_oom_read(void *buf, struct lmk_oom_read *rec)
{
	return -EINVAL;
}
static inline int memcheck_lmk_oom_write(const struct lmk_oom_write *write)
{
	return -EINVAL;
}
static inline int memcheck_report_lmk_oom(pid_t pid, pid_t tgid, const char *name,
			    enum kill_type ktype, short adj, size_t pss)
{
	return 0;
}
static inline int memcheck_createfs(void)
{
	return 0;
}
#endif /* CONFIG_DFX_MEMCHECK */
#endif /* _MEMCHECK_MEMSTAT_H */
