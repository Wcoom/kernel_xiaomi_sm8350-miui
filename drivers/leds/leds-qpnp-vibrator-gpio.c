// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved
 */

#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>

#define VIB_PLAY_MS        5000
#define VIB_MIN_PLAY_MS    50
#define VIB_MAX_PLAY_MS    15000
#define VIB_MAX_BRIGHTNESS 100

struct vib_chip {
	struct led_classdev  led_cdev;
	struct mutex         lock;
	struct pinctrl       *pin_gpio;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_active;
	struct hrtimer       stop_timer;
	struct work_struct   vib_work;

	u64                  vib_play_ms;
	unsigned int         vib_state;
	bool                 vib_enable;
};

static int vib_gpio_enable(struct vib_chip *chip, bool enable)
{
	int ret;

	if (chip == NULL) {
		pr_err("%s: chip NULL\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		ret = pinctrl_select_state(chip->pin_gpio, chip->pin_active);
	} else {
		ret = pinctrl_select_state(chip->pin_gpio, chip->pin_default);
	}

	if (ret) {
		pr_err("%s: pinctrl select fail, enable = %d, ret = %d\n", __func__, enable, ret);
		return ret;
	}

	chip->vib_enable = enable;
	return 0;
}

static void vib_work_func(struct work_struct *work)
{
	struct vib_chip *chip = NULL;

	chip = container_of(work, struct vib_chip, vib_work);
	mutex_lock(&chip->lock);
	if (chip->vib_state) {
		(void)vib_gpio_enable(chip, true);
		chip->vib_state = 0;

		hrtimer_start(&chip->stop_timer,
			ms_to_ktime(chip->vib_play_ms), HRTIMER_MODE_REL);
	} else {
		(void)vib_gpio_enable(chip, false);
	}
	mutex_unlock(&chip->lock);
}

static enum hrtimer_restart vib_stop_timer(struct hrtimer *timer)
{
	struct vib_chip *chip = NULL;

	chip = container_of(timer, struct vib_chip, stop_timer);
	schedule_work(&chip->vib_work);

	return HRTIMER_NORESTART;
}

/* sysfs file node */
static ssize_t state_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = NULL;
	struct vib_chip *chip = NULL;

	cdev = dev_get_drvdata(dev);
	chip = container_of(cdev, struct vib_chip, led_cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->vib_enable);
}

static ssize_t state_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static ssize_t activate_show(
	struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t activate_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct led_classdev *cdev = NULL;
	struct vib_chip *chip = NULL;

	cdev = dev_get_drvdata(dev);
	chip = container_of(cdev, struct vib_chip, led_cdev);

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		pr_err("%s: parse buf fail, ret = %d\n", __func__, ret);
		return ret;
	}

	hrtimer_cancel(&chip->stop_timer);
	cancel_work_sync(&chip->vib_work);

	mutex_lock(&chip->lock);
	chip->vib_state = val;
	mutex_unlock(&chip->lock);
	schedule_work(&chip->vib_work);

	pr_info("%s: sate = %u, time = %llums\n",
		__func__, val, chip->vib_play_ms);

	return count;
}

static ssize_t duration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = NULL;
	struct vib_chip *chip = NULL;
	ktime_t time_remain;
	s64 time_ms = 0;

	cdev = dev_get_drvdata(dev);
	chip = container_of(cdev, struct vib_chip, led_cdev);
	if (hrtimer_active(&chip->stop_timer)) {
		time_remain = hrtimer_get_remaining(&chip->stop_timer);
		time_ms = ktime_to_ms(time_remain);
	}

	return scnprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t duration_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	unsigned int val;
	struct led_classdev *cdev = NULL;
	struct vib_chip *chip = NULL;

	cdev = dev_get_drvdata(dev);
	chip = container_of(cdev, struct vib_chip, led_cdev);
	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		pr_err("%s: parse buf fail, ret = %d\n", __func__, ret);
		return ret;
	}

	val = (val < VIB_MIN_PLAY_MS) ? VIB_MIN_PLAY_MS : val;
	val = (val > VIB_MAX_PLAY_MS) ? VIB_MAX_PLAY_MS : val;

	mutex_lock(&chip->lock);
	chip->vib_play_ms = val;
	mutex_unlock(&chip->lock);

	return count;
}

static DEVICE_ATTR_RW(state);
static DEVICE_ATTR_RW(activate);
static DEVICE_ATTR_RW(duration);

static struct attribute *vib_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_activate.attr,
	&dev_attr_duration.attr,
	NULL
};

static const struct attribute_group vib_attr_group = {
	.attrs = vib_attributes,
};

static enum led_brightness vib_brightness_get(struct led_classdev *cdev)
{
	return 0;
}

static void vib_brightness_set(struct led_classdev *cdev, enum led_brightness level)
{
	struct vib_chip *chip = NULL;

	chip = container_of(cdev, struct vib_chip, led_cdev);
	if (chip == NULL) {
		pr_err("%s: chip NULL\n", __func__);
		return;
	}

	mutex_lock(&chip->lock);
	if (level) {
		(void)vib_gpio_enable(chip, true);
	} else {
		(void)vib_gpio_enable(chip, false);
	}
	mutex_unlock(&chip->lock);
}

static int pinctrl_init(struct device *dev, struct vib_chip *chip)
{
	if (dev == NULL || chip == NULL) {
		pr_err("%s: dev or chip is NULL\n", __func__);
		return -EINVAL;
	}

	chip->pin_gpio = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(chip->pin_gpio)) {
		pr_err("%s: pinctrl get fail\n", __func__);
		return -ENODEV;
	}

	chip->pin_default = pinctrl_lookup_state(chip->pin_gpio, "default");
	if (IS_ERR_OR_NULL(chip->pin_default)) {
		pr_err("%s: pinctrl lookup default state fail\n", __func__);
		return -EINVAL;
	}

	chip->pin_active = pinctrl_lookup_state(chip->pin_gpio, "active");
	if (IS_ERR_OR_NULL(chip->pin_active)) {
		pr_err("%s: pinctrl lookup active state fail\n", __func__);
		return -EINVAL;
	}

	if (pinctrl_select_state(chip->pin_gpio, chip->pin_default)) {
		pr_err("%s: pinctrl set default state fail\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int vibrator_probe(struct platform_device *pdev)
{
	int ret;
	struct vib_chip *chip = NULL;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		pr_err("%s: allocmem fail\n", __func__);
		return -ENOMEM;
	}

	ret = pinctrl_init(&pdev->dev, chip);
	if (ret) {
		pr_err("%s: pinctrl init fail\n", __func__);
		return ret;
	}

	chip->vib_play_ms = VIB_PLAY_MS;
	mutex_init(&chip->lock);
	INIT_WORK(&chip->vib_work, vib_work_func);

	hrtimer_init(&chip->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->stop_timer.function = vib_stop_timer;
	dev_set_drvdata(&pdev->dev, chip);

	chip->led_cdev.name = "vibrator";
	chip->led_cdev.brightness_get = vib_brightness_get;
	chip->led_cdev.brightness_set = vib_brightness_set;
	chip->led_cdev.max_brightness = VIB_MAX_BRIGHTNESS;
	ret = devm_led_classdev_register(&pdev->dev, &chip->led_cdev);
	if (ret < 0) {
		pr_err("%s: register led classdev fail\n", __func__);
		goto cdev_fail;
	}

	ret = sysfs_create_group(&chip->led_cdev.dev->kobj, &vib_attr_group);
	if (ret) {
		pr_err("%s: sysfs create fail\n", __func__);
		goto sysfs_fail;
	}

	pr_info("%s: success\n", __func__);
	return 0;

sysfs_fail:
	devm_led_classdev_unregister(&pdev->dev, &chip->led_cdev);

cdev_fail:
	mutex_destroy(&chip->lock);
	dev_set_drvdata(&pdev->dev, NULL);

	return ret;
}

static int vibrator_remove(struct platform_device *pdev)
{
	struct vib_chip *chip = dev_get_drvdata(&pdev->dev);

	if (chip == NULL) {
		pr_err("%s: chip NULL\n", __func__);
		return -EINVAL;
	}

	hrtimer_cancel(&chip->stop_timer);
	cancel_work_sync(&chip->vib_work);
	mutex_destroy(&chip->lock);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static int vibrator_suspend(struct device *dev)
{
	struct vib_chip *chip = NULL;

	chip = dev_get_drvdata(dev);
	if (chip == NULL) {
		pr_err("%s: chip NULL\n", __func__);
		return -EINVAL;
	}

	hrtimer_cancel(&chip->stop_timer);
	cancel_work_sync(&chip->vib_work);
	mutex_lock(&chip->lock);
	(void)vib_gpio_enable(chip, false);
	mutex_unlock(&chip->lock);

	return 0;
}
static SIMPLE_DEV_PM_OPS(vibrator_pm_ops, vibrator_suspend, NULL);

static const struct of_device_id vibrator_match_table[] = {
	{ .compatible = "qcom,qpnp-vibrator-gpio" },
	{ },
};
MODULE_DEVICE_TABLE(of, vibrator_match_table);

static struct platform_driver vibrator_driver = {
	.driver = {
		.name = "qcom,qpnp-vibrator-gpio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vibrator_match_table),
		.pm = &vibrator_pm_ops,
	},
	.probe = vibrator_probe,
	.remove = vibrator_remove,
};

module_platform_driver(vibrator_driver);

MODULE_DESCRIPTION("QPNP Vibrator-GPIO driver");
MODULE_LICENSE("GPL v2");
