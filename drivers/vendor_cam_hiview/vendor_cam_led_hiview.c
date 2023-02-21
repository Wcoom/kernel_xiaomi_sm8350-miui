/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 *
 * Description: hwcam hiview header file.
 *
 */
#include "vendor_cam_led_hiview.h"
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include "chipset_common/camera/kernel_cam_hiview.h"

#define PFX "[camhiview]"
#define log_dbg(fmt, args...) pr_debug(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_inf(fmt, args...) pr_info(PFX "%s %d " fmt, __func__, __LINE__, ##args)
#define log_err(fmt, args...) pr_err(PFX "%s %d " fmt, __func__, __LINE__, ##args)

static bool is_led_dmd_enable()
{
	struct device_node *of_node = NULL;
	bool ret = false;

	of_node = of_find_node_by_name(NULL, "product_name_camera");
	if (!of_node) {
		log_err("of_node is null");
		return false;
	}

	ret = of_property_read_bool(of_node, "led-dmd-enable");
	if (!ret)
		log_err("led-dmd-enable is not found");
	else
		log_dbg("led-dmd-enable(%d)", ret);

	return ret;
}

static void led_hiview_handle(int error_type)
{
	struct camera_dmd_info cam_info;

	if (!is_led_dmd_enable())
		return;

	if (hiview_init(&cam_info)) {
		log_err("led hiview init failed");
		return;
	}

	cam_info.error_type = error_type;
	hiview_report(&cam_info);
}

void vendor_cam_led_open_short_hiview_report(void)
{
	led_hiview_handle(LED_OPEN_SHORT_ERR);
}
