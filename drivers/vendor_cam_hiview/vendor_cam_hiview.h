/*
 * Copyright (c) 2021-2021 Huawei Technologies Co., Ltd. All Rights Reserved.
 * Description: hwcam hiview header file.
 *
 */
#ifndef HWCAM_HIVIEW_H
#define HWCAM_HIVIEW_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <chipset_common/camera/cam_hiview.h>
#include <cam_sensor_io.h>
#include "cam_sensor_dev.h"
#include "cam_actuator_dev.h"
#include "cam_eeprom_dev.h"
#include "cam_ois_dev.h"

/**
 * @error_type: dmd error type
 * @s_ctrl: Sensor ctrl structure
 * @error_scene: dmd error scene
 *
 * This API handles the camera sensor dmd error report
 */
void vendor_cam_hiview_handle(int error_type,
	struct cam_sensor_ctrl_t* s_ctrl, const char* error_scene);

/**
 * @error_type: i2c dmd error type
 * @io_master_info: i2c information structure
 * @error_scene: dmd error scene
 *
 * This API handles the camera sensor dmd error report
 */
void vendor_cam_i2c_hiview_handle(struct camera_io_master *io_master_info,
	const char* error_scene);

#endif // HWCAM_HIVIEW_H
