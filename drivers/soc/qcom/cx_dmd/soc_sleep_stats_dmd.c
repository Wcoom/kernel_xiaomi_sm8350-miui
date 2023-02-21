/*
 * soc_sleep_stats_dmd.c
 *
 * cx none idle dmd upload
 *
 * Copyright (C) 2017-2021 Huawei Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <securec.h>
#include <asm/arch_timer.h>
#include <log/hiview_hievent.h>

#define CXSD_NOT_IDLE_DMD 925004501
#define SPI_NOT_IDLE_DMD  925004510

#define REPORT_NONEIDLE_DMD_TIME (60 * 60 * 1000) /* 60min */
#define APSS_CRYSTAL_FREQ 19200000
#define APSS_REPORT_NONEIDLE_DMD_TIME (60 * 60) /* 60min */

static __le64 cx_last_acc_dur;
static s64 cx_last_time;
static bool flag_cx_none_idle, flag_cx_none_idle_short;

#define REPORT_SPI_UNSUSPEND_DMD_TIME (60 * 60 * 1000) /* 60min */
#define REPORT_SPI_UNSUSPEND_DMD_LIMIT_TIME (24 * 60 * 60 * 1000) /* 24hour */
#define REPORT_SPI_UNSUSPEND_CNT 40

static void soc_sleep_stats_dmd_report(int domain, const char* context)
{
	int dmd_code, ret;
	struct hiview_hievent *hi_event = NULL;

	dmd_code = domain;

	hi_event = hiview_hievent_create(dmd_code);
	if (!hi_event) {
		pr_err("create hievent fail\n");
		return;
	}

	ret = hiview_hievent_put_string(hi_event, "CONTENT", context);
	if (ret < 0)
		pr_err("hievent put string failed\n");

	ret = hiview_hievent_report(hi_event);
	if (ret < 0)
		pr_err("report hievent failed\n");

	hiview_hievent_destroy(hi_event);
}

void cx_dmd_check_apss_state(const uint64_t acc_dur, const uint64_t last_enter, const uint64_t last_exit)
{
	static bool apss_first = true;
	static uint64_t last_apss_sleep_acc_dur;
	uint64_t cur_apss_sleep_dur;
	uint64_t accumulated_duration = acc_dur;

	/*
	* If a master is in sleep when reading the sleep stats from SMEM
	* adjust the accumulated sleep duration to show actual sleep time.
	* This ensures that the displayed stats are real when used for
	* the purpose of computing battery utilization.
	*/
	if (last_enter > last_exit)
		accumulated_duration +=
#ifndef QCOM_PLATFORM_6225
			(__arch_counter_get_cntvct()
			- last_enter);
#else
			(arch_counter_get_cntvct()
			- last_enter);
#endif

	if (apss_first) {
		last_apss_sleep_acc_dur = accumulated_duration;
		apss_first = false;
		return;
	}

	if (accumulated_duration < last_apss_sleep_acc_dur) {
		last_apss_sleep_acc_dur = accumulated_duration;
		cx_last_time = ktime_to_ms(ktime_get_real());
		return;
	}

	if (flag_cx_none_idle_short || flag_cx_none_idle) {
		if (flag_cx_none_idle) {
			cur_apss_sleep_dur = accumulated_duration - last_apss_sleep_acc_dur;
			cur_apss_sleep_dur = cur_apss_sleep_dur / APSS_CRYSTAL_FREQ; // to second

			if (cur_apss_sleep_dur >= APSS_REPORT_NONEIDLE_DMD_TIME) {
				soc_sleep_stats_dmd_report(CXSD_NOT_IDLE_DMD, "cx none idle");

				flag_cx_none_idle = false;
				flag_cx_none_idle_short = false;
				cx_last_time = ktime_to_ms(ktime_get_real());
				last_apss_sleep_acc_dur = accumulated_duration;
			}
		}
	} else {
		last_apss_sleep_acc_dur = accumulated_duration;
	}
}

void check_cx_idle_state(const __le64 cur_acc_duration, const s64 now)
{
	static bool cx_first = true;
	s64 none_idle_time;

	if (cx_first) {
		cx_last_time = now;
		cx_last_acc_dur = cur_acc_duration;
		cx_first = false;
		return;
	}

	if (cur_acc_duration != cx_last_acc_dur) {
		cx_last_acc_dur = cur_acc_duration;
		cx_last_time = now;
		flag_cx_none_idle = false;
		flag_cx_none_idle_short = false;
	} else {
		none_idle_time = now - cx_last_time;
		if (none_idle_time >= REPORT_NONEIDLE_DMD_TIME) {
			flag_cx_none_idle = true;
			flag_cx_none_idle_short = false;
		} else {
			flag_cx_none_idle_short = true;
			flag_cx_none_idle = false;
		}
	}
}

void check_spi_idle_state(const s64 now)
{
	static bool spi_first = true;
	static bool spi_report_dmd = false;
	static uint64_t spi_unsuspend_cnt;
	static uint64_t last_spi_dmd_report_time;
	static s64 spi_first_time;

	if (spi_first) {
		spi_first_time = now;
		spi_first = false;
		return;
	}

	if ((spi_report_dmd == true) && (now - last_spi_dmd_report_time < REPORT_SPI_UNSUSPEND_DMD_LIMIT_TIME)) {
		return;
	}

	if (now - spi_first_time < REPORT_SPI_UNSUSPEND_DMD_TIME) {
		spi_unsuspend_cnt++;
		if (spi_unsuspend_cnt > REPORT_SPI_UNSUSPEND_CNT) {
			last_spi_dmd_report_time = now;
			soc_sleep_stats_dmd_report(SPI_NOT_IDLE_DMD, "spi none idle");
			spi_report_dmd = true;
			spi_unsuspend_cnt = 0;
		}
	} else {
		spi_first_time = now;
		spi_unsuspend_cnt = 0;
	}
}