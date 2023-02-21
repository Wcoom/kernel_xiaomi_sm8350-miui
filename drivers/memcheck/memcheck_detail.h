/*
 * memcheck_detail.h
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

#ifndef _MEMCHECK_DETAIL_H
#define _MEMCHECK_DETAIL_H

#include <linux/types.h>
#include "memcheck_ioctl.h"
#include "impl/memdetail_imp.h"

#ifdef CONFIG_DFX_MEMCHECK_DETAIL
int memcheck_detail_read(void *buf, struct detail_info *info);
int memcheck_native_detail_write(const void *buf,
				 const struct detail_info *info);
int memcheck_slub_detail_preread(void);
#else /* CONFIG_DFX_MEMCHECK_DETAIL */
static inline int memcheck_detail_read(void *buf, struct detail_info *info)
{
	return -EINVAL;
}
static inline int memcheck_native_detail_write(void *buf,
					       struct detail_info *info)
{
	return -EINVAL;
}
static inline int memcheck_slub_detail_preread(void)
{
	return -EINVAL;
}
#endif /* CONFIG_DFX_MEMCHECK_DETAIL */
#endif /* _MEMCHECK_DETAIL_H */
