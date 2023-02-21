/*
 * cpld Driver
 *
 * Copyright (c) 2012-2022 Huawei Technologies Co., Ltd.
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


#include "cpld_driver.h"
#if defined(CONFIG_HUAWEI_DSM)
#include <dsm/dsm_pub.h>
#endif

#define CPLD_DEVICE_NAME "huawei_cpld"

#define I2C_RW_RETRY_NUM 3
#define XFER_NUM 2
#define I2C_WAIT_TIME 25
#define CPLD_FW_UPDATE_RESULT 128


#define HWLOG_TAG CPLD
HWLOG_REGIST();

#define cpld_log_info(x...) _hwlog_info(HWLOG_TAG, ##x)
#define cpld_log_err(x...) _hwlog_err(HWLOG_TAG, ##x)
#define cpld_log_debug(x...) do { \
		if (1) \
			_hwlog_info(HWLOG_TAG, ##x); \
	} while (0)

static struct cpld_core_data *g_cpld_core_data;
static char cpld_result[CPLD_FW_UPDATE_RESULT] = {0};

#if defined(CONFIG_HUAWEI_DSM)
#define CPLD_CHIP_DMD_REPORT_SIZE 256
#define CPLD_DSM_BUFF_SIZE 1024

static struct dsm_dev dsm_cpld = {
	.name = "dsm_cpld",
	.device_name = "CPLD",
	.ic_name = "GW",
	.module_name = "CPLD",
	.fops = NULL,
	.buff_size = CPLD_DSM_BUFF_SIZE,
};

struct dsm_client *dsm_cpld_client;

void cpld_dmd_report(int dmd_num, const char *format, ...)
{
	va_list args;
	char *input_buf = kzalloc(CPLD_CHIP_DMD_REPORT_SIZE, GFP_KERNEL);
	char *report_buf = kzalloc(CPLD_CHIP_DMD_REPORT_SIZE, GFP_KERNEL);

	if ((!input_buf) || (!report_buf)) {
		cpld_log_err("%s: memory is not enough!!\n", __func__);
		goto exit;
	}

	va_start(args, format);
	vsnprintf(input_buf, CPLD_CHIP_DMD_REPORT_SIZE - 1, format, args);
	va_end(args);
	snprintf(report_buf, CPLD_CHIP_DMD_REPORT_SIZE - 1,
		"cpld:%s\n", input_buf);

	if (!dsm_client_ocuppy(dsm_cpld_client)) {
		dsm_client_record(dsm_cpld_client, report_buf);
		dsm_client_notify(dsm_cpld_client, dmd_num);
		cpld_log_err("%s: %s\n", __func__, report_buf);
	}

exit:
	kfree(input_buf);
	kfree(report_buf);
}
EXPORT_SYMBOL(cpld_dmd_report);
#endif

static void cpld_cmdline_fw_update_status(void)
{
	char *res = NULL;
	char *temp = NULL;
	int len = 0;

	res = strstr(saved_command_line, "cpld_update_failed_errcode");
	if (res) {
		temp = res;
		while ((*temp != ' ') && (len < CPLD_FW_UPDATE_RESULT)) {
			len++;
			temp++;
		}
		if ((len > 0) && (len < CPLD_FW_UPDATE_RESULT)) {
			memcpy(cpld_result, res, len);
			cpld_dmd_report(DSM_CPLD_FW_UPDATE_ERROR_NO, "cpld_result:%s",
				cpld_result);
			cpld_log_err("%s:cpld update result: %s\n", __func__, cpld_result);
		}
		return;
	}
	cpld_log_info("%s:cpld fw update success\n", __func__);
}

static int cpld_parse_config(struct i2c_client *i2c_client,
	struct cpld_core_data *cd)
{
	struct device_node *np = i2c_client->dev.of_node;
	int ret;

	ret = of_property_read_u32(np, "fw_update_type", &cd->fw_update_type);
	if (ret) {
		cpld_log_err("%s:get fw_update_type failed!\n", __func__);
		return -EINVAL;
	}
	cpld_log_info("%s:fw_update_type = 0x%x\n", __func__, cd->fw_update_type);
	if (cd->fw_update_type == NOT_SUPPORT_CPLD) {
		cpld_log_err("%s:not support cpld\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "i2c_address", &cd->i2c_address);
	if (ret) {
		cpld_log_err("%s:get i2c address failed!\n", __func__);
		return -EINVAL;
	}
	cpld_log_info("%s:i2c address = 0x%x\n", __func__, cd->i2c_address);
	i2c_client->addr = cd->i2c_address;

	cd->mode1_gpio = of_get_named_gpio(np, "mode1_gpio", 0);
	cpld_log_info("irq mode1_gpio = %d\n", cd->mode1_gpio);
	if (!gpio_is_valid(cd->mode1_gpio)) {
		cpld_log_err("%s: get mode1_gpio failed\n", __func__);
		return -EINVAL;
	}

	cd->mode2_gpio = of_get_named_gpio(np, "mode2_gpio", 0);
	cpld_log_info("mode2_gpio = %d\n", cd->mode1_gpio);
	if (!gpio_is_valid(cd->mode2_gpio)) {
		cpld_log_err("%s: get mode2_gpio failed\n", __func__);
		return -EINVAL;
	}
	cpld_log_info("%s:success\n", __func__);
	return 0;
}

static int cpld_i2c_read(u8 *reg_addr, u16 reg_len, u8 *buf, u16 len)
{
	struct cpld_core_data *cd = g_cpld_core_data;
	int count = 0;
	int ret;
	int msg_len;
	struct i2c_msg *msg_addr = NULL;
	struct i2c_msg xfer[XFER_NUM];

	/* register addr */
	xfer[0].addr = cd->client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reg_len;
	xfer[0].buf = reg_addr;

	/* Read data */
	xfer[1].addr = cd->client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = buf;

	if (reg_len > 0) {
		msg_len = XFER_NUM;
		msg_addr = &xfer[0];
	} else {
		msg_len = 1;
		msg_addr = &xfer[1];
	}
	do {
		ret = i2c_transfer(cd->client->adapter,
			msg_addr, msg_len);
		if (ret == msg_len) {
			return 0;
		}
		cpld_log_err("%s:i2c read status: %d\n", __func__, ret);
		msleep(I2C_WAIT_TIME);
	} while (++count < I2C_RW_RETRY_NUM);

	cpld_log_err("%s failed\n", __func__);
	cpld_dmd_report(DSM_CPLD_I2C_ERROR_NO, "cpld i2c error: %d", ret);
	return -EIO;
}

static int cpld_get_fw_version(struct cpld_core_data *cpld_data)
{
	int ret;
	unsigned char main_fw_version = 0;
	unsigned char slave_fw_version = 0;
	char main_reg_addr = 0x04;
	char slave_reg_addr = 0x05;

	ret = cpld_i2c_read(&main_reg_addr, 1, &main_fw_version, 1);
	cpld_log_info("%s:get 0x04 reg value = %x, ret = %d\n", __func__, main_fw_version, ret);
	if (ret < 0) {
		cpld_log_info("%s:get main fw_version failed, Maybe the firmware is corrupted\n", __func__);
		return ret;
	}
	ret = cpld_i2c_read(&slave_reg_addr, 1, &slave_fw_version, 1);
	cpld_log_info("%s:get 0x05 reg value = %x, ret = %d\n", __func__, slave_fw_version, ret);
	if (ret < 0) {
		cpld_log_info("%s:get alave fw_version failed, Maybe the firmware is corrupted\n", __func__);
		return ret;
	}
	cpld_data->main_usercode = main_fw_version;
	cpld_data->slave_usercode = slave_fw_version;
	return 0;
}

static ssize_t cpld_fw_update_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE - 1, "fw_update_type = %u\n", g_cpld_core_data->fw_update_type);
}

static ssize_t cpld_fw_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE - 1, "%u\n", g_cpld_core_data->fw_status);
}

static ssize_t cpld_fw_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE - 1, "main = 0x%x, slave = 0x%x\n",
		g_cpld_core_data->main_usercode, g_cpld_core_data->slave_usercode);
}

static ssize_t cpld_change_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int status;
	int gpio_num = 0;

	ret = sscanf(buf, "%1u", &status);
	if (ret <= 0) {
		cpld_log_err("%s: illegal input, ret = %d\n", __func__, ret);
		return -EINVAL;
	}
	if (status == MAIN_CPLD_NUM)
		gpio_num = g_cpld_core_data->mode1_gpio;
	else if (status == SLAVE_CPLD_NUM)
		gpio_num = g_cpld_core_data->mode2_gpio;
	else
		return count;

	gpio_direction_output(gpio_num, 1);
	mdelay(50);
	gpio_direction_output(gpio_num, 0);
	mdelay(10);
	cpld_log_err("%s: change %d mode, gpio_num = %d\n",
		__func__, status, gpio_num);
	return count;
}

static struct device_attribute fw_version_file = __ATTR(fw_version,
	0440, cpld_fw_version_show, NULL);
static struct device_attribute fw_update_type_file = __ATTR(fw_update_type,
	0440, cpld_fw_update_type_show, NULL);
static struct device_attribute cpld_fw_status_file = __ATTR(cpld_fw_status,
	0444, cpld_fw_status_show, NULL);
static struct device_attribute cpld_change_mode_file = __ATTR(cpld_change_mode,
	0220, NULL, cpld_change_mode_store);

static struct platform_device cpld_device = {
	.name = "cpld_device",
	.id = -1,
};

static int create_cpld_device(struct cpld_core_data *cpld_data)
{
	int ret;

	ret = platform_device_register(&cpld_device);
	if (ret) {
		cpld_log_info("%s: platform_device_register failed, ret:%d\n",
			__func__, ret);
		return -EINVAL;
	}
	if (device_create_file(&cpld_device.dev, &fw_version_file)) {
		cpld_log_err("%s:Unable to create fw_version interface\n", __func__);
		return -EINVAL;
	}
	if (device_create_file(&cpld_device.dev, &cpld_fw_status_file)) {
		cpld_log_err("%s:Unable to create fw_update_type interface\n", __func__);
		return -EINVAL;
	}
	if (device_create_file(&cpld_device.dev, &fw_update_type_file)) {
		cpld_log_err("%s:Unable to create fw_update_type interface\n", __func__);
		return -EINVAL;
	}
	if (device_create_file(&cpld_device.dev, &cpld_change_mode_file)) {
		cpld_log_err("%s:Unable to create fw_update_type interface\n", __func__);
		return -EINVAL;
	}

	cpld_log_info("%s:out\n", __func__);
	return 0;
}

static int cpld_gpio_init(struct cpld_core_data *cpld_data)
{
	int rc;

	rc = gpio_request(cpld_data->mode1_gpio, "cpld_mode1");
	if (rc)
		cpld_log_err("%s:gpio_request %d failed\n", __func__,
			cpld_data->mode1_gpio);

	rc += gpio_request(cpld_data->mode2_gpio, "cpld_mode2");
	if (rc)
		cpld_log_err("%s: gpio %d request failed\n", __func__,
			cpld_data->mode2_gpio);

	cpld_log_info("%s:out, %d\n", __func__, rc);
	return rc;
}

void cpld_start_wd_timer(struct cpld_core_data *cpld_data)
{
	cpld_log_info("%s:start wd\n", __func__);
	mod_timer(&cpld_data->watchdog_timer,
		jiffies + msecs_to_jiffies(20 * 1000));
}

void cpld_stop_wd_timer(struct cpld_core_data *cpld_data)
{
	cpld_log_info("%s:stop wd\n", __func__);
	del_timer(&cpld_data->watchdog_timer);
	cancel_work_sync(&cpld_data->watchdog_work);
	del_timer(&cpld_data->watchdog_timer);
}

static void cpld_report_fw_version(void)
{
	unsigned char cpld_fw_info[FW_INFO_LEN + 1] = {0};
	struct cpld_core_data *cd = g_cpld_core_data;

	snprintf(cpld_fw_info, FW_INFO_LEN,
		"main version=0x%x, slave version=0x%x, fw_update_type = %u",
		cd->main_usercode, cd->slave_usercode, cd->fw_update_type);
	cpld_dmd_report(DSM_CPLD_FW_VERSION_NO, "cpld fw version:%s", cpld_fw_info);
	cpld_log_info("%s: %s\n", __func__, cpld_fw_info);
}

static void cpld_report_sync_err_dmd(unsigned char main_value, unsigned char slave_value)
{
	unsigned char cpld_sync_err_info[CPLD_SYNC_ERR_INFO_LEN + 1] = {0};

	snprintf(cpld_sync_err_info, CPLD_SYNC_ERR_INFO_LEN, "main value=%u, slave value=%u",
		main_value, slave_value);
	cpld_dmd_report(DSM_CPLD_SYNC_ERROR_NO, "cpld sync err:%s", cpld_sync_err_info);
	cpld_log_info("%s: %s\n", __func__, cpld_sync_err_info);
}

#define REPORT_DMD_DELAY 2
static void cpld_watchdog_work(struct work_struct *work)
{
	int ret;
	struct cpld_core_data *cd = g_cpld_core_data;
	unsigned char main_value = 0;
	unsigned char slave_value = 0;
	char main_sync_reg = 0x06;
	char slave_sync_reg = 0x07;
	static int i = 0;

	__pm_stay_awake(cd->cpld_wake_lock);
	ret = cpld_i2c_read(&main_sync_reg, 1, &main_value, 1);
	cpld_log_info("%s:reg 0x%x value = 0x%x, ret = %d\n", __func__, main_sync_reg, main_value, ret);
	if (ret < 0)
		cpld_log_info("%s:get main sync error reg failed\n", __func__);

	ret = cpld_i2c_read(&slave_sync_reg, 1, &slave_value, 1);
	cpld_log_info("%s:reg 0x%x value = 0x%x, ret = %d\n", __func__, slave_sync_reg, slave_value, ret);
	if (ret < 0)
		cpld_log_info("%s:get slave sync error reg failed\n", __func__);

	if (i == REPORT_DMD_DELAY) {
		cpld_cmdline_fw_update_status();
		cpld_report_fw_version();
	}

	i++;

	if ((((main_value & 0xF0) >> 4) >= CPLD_MIAN_SYNC_ERR_NUM_LIMIT) ||
		((main_value & 0x0F) >= CPLD_MIAN_SYNC_ERR_NUM_LIMIT) ||
		(slave_value >= CPLD_SLAVE_SYNC_ERR_NUM_LIMIT)) {
		cd->fw_status = CPLD_FW_SYNC_ERR;
		if ((main_value == cd->main_sync_err_num) &&
			(slave_value == cd->slave_sync_err_num)) {
			cpld_log_err("%s:cpld sync err duplicate err, goto exit\n", __func__);
			goto exit;
		}
		cpld_report_sync_err_dmd(main_value, slave_value);
		cpld_log_err("%s:cpld sync err, report dmd\n", __func__);
	}
	cd->fw_status = CPLD_NO_ERR;
	cd->main_sync_err_num = main_value;
	cd->slave_sync_err_num = slave_value;

exit:
	cpld_start_wd_timer(cd);
	__pm_relax(cd->cpld_wake_lock);
}

static void cpld_watchdog_timer(struct timer_list *t)
{
	if (!work_pending(&g_cpld_core_data->watchdog_work))
		schedule_work(&g_cpld_core_data->watchdog_work);
}

static int cpld_watchdog_init(struct cpld_core_data *cpld_data)
{
	cpld_data->cpld_wake_lock = wakeup_source_register(&cpld_data->client->dev, "cpld_wake_lock");
	if (!cpld_data->cpld_wake_lock) {
		cpld_log_err("%s: failed to init wakelock\n", __func__);
		return -EINVAL;
	}
	INIT_WORK(&(cpld_data->watchdog_work), cpld_watchdog_work);
	timer_setup(&(cpld_data->watchdog_timer), cpld_watchdog_timer, 0);
	cpld_start_wd_timer(cpld_data);
	return 0;
}

static int create_platform_init(struct cpld_core_data *cpld_data)
{
	int ret;

	ret = cpld_gpio_init(cpld_data);
	if (ret) {
		cpld_log_err("%s:platform data is required!\n", __func__);
		return ret;
	}
	ret = create_cpld_device(cpld_data);
	if (ret) {
		cpld_log_err("%s:platform data is required!\n", __func__);
		return ret;
	}
	(void)cpld_get_fw_version(cpld_data);
	ret = cpld_watchdog_init(cpld_data);
	cpld_log_info("%s:watchdog init out, ret = %d\n", __func__, ret);
	return ret;
}

static int cpld_probe(struct i2c_client *i2c_client,
	const struct i2c_device_id *id)
{
	struct cpld_core_data *cpld_data = NULL;
	int ret;

	cpld_log_info("%s: enter\n", __func__);

	cpld_data = kzalloc(sizeof(struct cpld_core_data), GFP_KERNEL);
	if (!cpld_data) {
		cpld_log_err("%s:platform data is required!\n", __func__);
		return -EINVAL;
	}

	cpld_data->client = i2c_client;
	g_cpld_core_data = cpld_data;
#if defined(CONFIG_HUAWEI_DSM)
	dsm_cpld_client = dsm_register_client(&dsm_cpld);
#endif
	ret = cpld_parse_config(i2c_client, cpld_data);
	if (ret) {
		cpld_log_err("%s:cpld parse config failed\n", __func__);
		goto create_cpld_device_err;
	}

	ret = create_platform_init(cpld_data);
	cpld_log_info("%s: out, ret = %d\n", __func__, ret);
	return 0;
create_cpld_device_err:
	kfree(cpld_data);
	g_cpld_core_data = NULL;
	return 0;
}

static int cpld_remove(struct i2c_client *client)
{
	cpld_log_info("%s: out\n", __func__);
	return 0;
}

static int cpld_suspend(struct device *dev)
{
	if (!g_cpld_core_data)
		return 0;
	cpld_log_info("%s:enter\n", __func__);
	cpld_stop_wd_timer(g_cpld_core_data);
	g_cpld_core_data->main_sync_err_num = 0;
	g_cpld_core_data->slave_sync_err_num = 0;
	cpld_log_info("%s:out\n", __func__);
	return 0;
}

static int cpld_resume(struct device *dev)
{
	if (!g_cpld_core_data)
		return 0;
	cpld_log_info("%s:enter\n", __func__);
	cpld_start_wd_timer(g_cpld_core_data);
	cpld_log_info("%s:out\n", __func__);
	return 0;
}

static const struct dev_pm_ops cpld_pm_ops = {
	.prepare = cpld_suspend,
	.resume = cpld_resume,
};
static const struct of_device_id g_cpld_match_table[] = {
	{ .compatible = "huawei,cpld", },
	{ },
};

MODULE_DEVICE_TABLE(of, g_cpld_match_table);

static const struct i2c_device_id g_cpld_device_id[] = {
	{ CPLD_DEVICE_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, g_cpld_device_id);

static struct i2c_driver g_huawei_cpld_driver = {
	.probe = cpld_probe,
	.remove = cpld_remove,
	.id_table = g_cpld_device_id,
	.driver = {
		.name = CPLD_DEVICE_NAME,
		.owner = THIS_MODULE,
		.pm = &cpld_pm_ops,
		.of_match_table = g_cpld_match_table,
	},
};

static int __init cpld_i2c_init(void)
{
	int ret = 0;

	cpld_log_info("%s: enter\n", __func__);
	ret = i2c_add_driver(&g_huawei_cpld_driver);
	if (ret) {
		cpld_log_err("%s: error\n", __func__);
		return ret;
	}
	return 0;
}

module_init(cpld_i2c_init);


static void __exit cpld_i2c_exit(void)
{
	i2c_del_driver(&g_huawei_cpld_driver);
}

module_exit(cpld_i2c_exit);

MODULE_AUTHOR("Huawei Device Company");
MODULE_DESCRIPTION("Huawei cpld Driver");
MODULE_LICENSE("GPL");

