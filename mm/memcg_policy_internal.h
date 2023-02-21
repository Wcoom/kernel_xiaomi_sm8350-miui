/*
 * memcg_reclaim_policy.h
 *
 * Copyright(C) 2020 Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef _MEMCG_POLICY_INTERNAL_H
#define _MEMCG_POLICY_INTERNAL_H

struct mem_cgroup;
struct seqfile;
struct pglist_data;

unsigned long reclaim_anon_memcg(struct pglist_data *,
		struct mem_cgroup *, bool);
unsigned long reclaim_all_anon_memcg(struct pglist_data *,
		struct mem_cgroup *);
#ifdef CONFIG_RAMTURBO
inline bool is_zram_anon_comp_enabled(void);
unsigned long all_active_reclaim_by_memcg(struct pglist_data *,
		struct mem_cgroup *);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
void set_hyperhold_swappiness(int);
int get_hyperhold_swappiness(void);
bool is_swap_full(void);
#ifdef CONFIG_HUAWEI_DIRECT_SWAPPINESS
void set_hyperhold_dr_swappiness(int);
int get_hyperhold_dr_swappiness(void);
#endif
#endif

inline bool get_ec_app_start_flag_value(void);
inline bool get_eswap_switch_value(void);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
inline u64 get_swap_full_ratio(void);
#endif

#ifdef CONFIG_HYPERHOLD_ZSWAPD
enum zswapd_pressure_level {
	LEVEL_LOW = 0,
	LEVEL_MEDIUM,
	LEVEL_CRITICAL,
	LEVEL_COUNT
};
enum zswapd_eswap_policy {
	CHECK_BUFFER_ONLY = 0,
	CHECK_BUFFER_ZRAMRAITO_BOTH
};
void zswapd_pressure_report(enum zswapd_pressure_level level);
inline u64 get_zram_wm_ratio_value(void);
inline u64 get_compress_ratio_value(void);
inline unsigned int get_avail_buffers_value(void);
inline unsigned int get_min_avail_buffers_value(void);
inline unsigned int get_high_avail_buffers_value(void);
inline u64 get_zswapd_max_reclaim_size(void);
inline unsigned int get_inactive_file_ratio_value(void);
inline unsigned int get_active_file_ratio_value(void);
inline unsigned long long get_area_anon_refault_threshold_value(void);
inline unsigned long get_anon_refault_snapshot_min_interval_value(void);
inline unsigned long long get_empty_round_skip_interval_value(void);
inline unsigned long long get_max_skip_interval_value(void);
inline unsigned long long get_empty_round_check_threshold_value(void);
inline u64 get_zram_critical_threshold_value(void);
#endif

#endif
