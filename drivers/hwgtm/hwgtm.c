

#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/loadavg.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/time.h>

#include <huawei_platform/log/hw_log.h>

#define HWGTM_TASK_LIST_SIZE		10
#define HWGTM_RECENT_EVENT_THRESHOLD_NS	2000000000
#define HWGTM_TASK_TIMEOUT		2000000000
#define HWGTM_TASK_TYPE_VACANT		-1
#define HWGTM_TASK_TYPE_NUM		2
#define HWGTM_TASK_DELAY_MS		1000

#define HWGTM_IOCTL_MAGIC		0xBE
#define HWGTM_SYS_EVENT			_IOW(HWGTM_IOCTL_MAGIC, 1, int32_t)
#define HWGTM_REQUEST_TASK		_IOR(HWGTM_IOCTL_MAGIC, 2, int32_t)
#define HWGTM_START_TASK		_IOR(HWGTM_IOCTL_MAGIC, 3, struct user_task_info)
#define HWGTM_FINISH_TASK		_IOR(HWGTM_IOCTL_MAGIC, 4, struct user_task_info)

#define HWGTM_SYS_EVENT_APP_START_BEGIN	1
#define HWGTM_SYS_EVENT_APP_START_END	2
#define HWGTM_SYS_EVENT_NUM		10

#undef HWLOG_TAG
#define HWLOG_TAG hwgtm
HWLOG_REGIST();

#define hwgtm_loge(fmt, ...) do { \
	hwlog_err("%s "fmt"\n", __func__, ##__VA_ARGS__); \
	} while (0)

#define hwgtm_logw(fmt, ...) do { \
	hwlog_warn("%s "fmt"\n", __func__, ##__VA_ARGS__); \
	} while (0)

#define hwgtm_logi(fmt, ...) do { \
	hwlog_info("%s "fmt"\n", __func__, ##__VA_ARGS__); \
	} while (0)

#define hwgtm_logd(fmt, ...) do { \
	hwlog_debug("%s "fmt"\n", __func__, ##__VA_ARGS__); \
	} while (0)

struct user_task_info {
	int32_t type;
	int32_t id;
};

struct hwgtm_task_info {
	struct user_task_info user_data;
	uint64_t start_time;
};

struct hwgtm_info {
	spinlock_t gtm_lock;
	ktime_t events_last_time[HWGTM_SYS_EVENT_NUM];
	int32_t hwgtm_task_count[HWGTM_TASK_TYPE_NUM];
	struct hwgtm_task_info hwgtm_task_container[HWGTM_TASK_TYPE_NUM][HWGTM_TASK_LIST_SIZE];
};

static struct hwgtm_info *g_hwgtm_info = NULL;

static inline void hwgtm_reset_task_locked(struct hwgtm_task_info *tinfo)
{
	tinfo->user_data.type = HWGTM_TASK_TYPE_VACANT;
}

static int hwgtm_clean_tasks_locked(ktime_t curr_time, int32_t index)
{
	int i;
	ktime_t elapsed_time;
	int available_task_count = HWGTM_TASK_LIST_SIZE;
	for (i = 0; i < HWGTM_TASK_LIST_SIZE; i++) {
		if (g_hwgtm_info->hwgtm_task_container[index][i].user_data.type != HWGTM_TASK_TYPE_VACANT) {
			available_task_count--;
			elapsed_time = ktime_sub(curr_time, g_hwgtm_info->hwgtm_task_container[index][i].start_time);
			if (elapsed_time >= HWGTM_TASK_TIMEOUT) {
				hwgtm_reset_task_locked(&g_hwgtm_info->hwgtm_task_container[index][i]);
				available_task_count++;
			}
		}
	}
	return available_task_count;
}

static long hwgtm_system_event(unsigned long arg)
{
	int32_t event;
	int32_t __user *uarg = (int32_t __user *)arg;
	ktime_t event_time;

	if (g_hwgtm_info == NULL)
		return -ENODEV;
	if (copy_from_user(&event, uarg, sizeof(event)))
		return -EFAULT;
	if (event >= HWGTM_SYS_EVENT_NUM)
		return -EINVAL;

	event_time = ktime_get();
	spin_lock(&g_hwgtm_info->gtm_lock);
	if (event == HWGTM_SYS_EVENT_APP_START_BEGIN) {
		g_hwgtm_info->events_last_time[event] = event_time;
	} else if (event == HWGTM_SYS_EVENT_APP_START_END) {
		g_hwgtm_info->events_last_time[event] = event_time;
		g_hwgtm_info->events_last_time[HWGTM_SYS_EVENT_APP_START_BEGIN] = 0;
	}
	spin_unlock(&g_hwgtm_info->gtm_lock);

	return 0;
}

static long hwgtm_request_task(unsigned long arg)
{
	ktime_t curr_time;
	ktime_t elapsed_time;
	int32_t ret_delay;
	int available_task_count;
	bool has_recent_event = false;
	bool heavy_load = false;
	int32_t __user *uarg = (int32_t __user *)arg;
	int i;

	if (g_hwgtm_info == NULL)
		return -ENODEV;

	ret_delay = 0;
	curr_time = ktime_get();
	spin_lock(&g_hwgtm_info->gtm_lock);
	elapsed_time = ktime_sub(curr_time, g_hwgtm_info->events_last_time[HWGTM_SYS_EVENT_APP_START_BEGIN]);
	if (elapsed_time < HWGTM_RECENT_EVENT_THRESHOLD_NS) {
		/* recent key event, need to delay */
		has_recent_event = true;
	} else {
		for (i = 0; i < HWGTM_TASK_TYPE_NUM; i++) {
			available_task_count = hwgtm_clean_tasks_locked(curr_time, i);
			if (available_task_count == 0) {
				/* heavy task pressure, need to delay */
				heavy_load = true;
				break;
			}
		}
	}
	spin_unlock(&g_hwgtm_info->gtm_lock);
	if (has_recent_event || heavy_load)
		ret_delay = HWGTM_TASK_DELAY_MS;
	if (put_user(ret_delay, uarg))
		return -EFAULT;

	return 0;
}

static void hwgtm_insert_task_locked(struct hwgtm_task_info *task_info)
{
	int i;
	ktime_t elapsed_time;
	struct hwgtm_task_info *tmp_info = NULL;
	int32_t insert_type = task_info->user_data.type;
	for (i = 0; i < HWGTM_TASK_LIST_SIZE; i++) {
		tmp_info = &g_hwgtm_info->hwgtm_task_container[insert_type][i];
		if (tmp_info->user_data.type == HWGTM_TASK_TYPE_VACANT) {
			g_hwgtm_info->hwgtm_task_container[insert_type][i] = *task_info;
			return;
		} else {
			elapsed_time = ktime_sub(task_info->start_time, tmp_info->start_time);
			if (elapsed_time >= HWGTM_TASK_TIMEOUT) {
				/* replace the timeout task with the new task */
				g_hwgtm_info->hwgtm_task_container[insert_type][i] = *task_info;
				return;
			}
		}
	}
	/* no vacant slot found, discard the new task */
	return;
}

static long hwgtm_start_task(unsigned long arg)
{
	struct hwgtm_task_info uinfo;
	void __user *uarg = (void __user *)arg;

	if (g_hwgtm_info == NULL)
		return -ENODEV;
	if (copy_from_user(&uinfo.user_data, uarg, sizeof(struct user_task_info)))
		return -EFAULT;
	if (uinfo.user_data.type < 0 || uinfo.user_data.type >= HWGTM_TASK_TYPE_NUM)
		return -EINVAL;

	uinfo.start_time = ktime_get();
	spin_lock(&g_hwgtm_info->gtm_lock);
	hwgtm_insert_task_locked(&uinfo);
	spin_unlock(&g_hwgtm_info->gtm_lock);
	return 0;
}

static long hwgtm_finish_task(unsigned long arg)
{
	int i;
	struct hwgtm_task_info uinfo;
	int32_t utask_type;
	int32_t utask_id;
	struct hwgtm_task_info *tmp_info = NULL;
	void __user *uarg = (void __user *)arg;

	if (g_hwgtm_info == NULL)
		return -ENODEV;
	if (copy_from_user(&uinfo.user_data, uarg, sizeof(struct user_task_info)))
		return -EFAULT;
	if (uinfo.user_data.type < 0 || uinfo.user_data.type >= HWGTM_TASK_TYPE_NUM)
		return -EINVAL;

	utask_type = uinfo.user_data.type;
	utask_id = uinfo.user_data.id;
	spin_lock(&g_hwgtm_info->gtm_lock);
	for (i = 0; i < HWGTM_TASK_LIST_SIZE; i++) {
		tmp_info = &g_hwgtm_info->hwgtm_task_container[utask_type][i];
		if (tmp_info->user_data.type != HWGTM_TASK_TYPE_VACANT &&
		    tmp_info->user_data.id == utask_id) {
			hwgtm_reset_task_locked(tmp_info);
			break;
		}
	}
	spin_unlock(&g_hwgtm_info->gtm_lock);
	return 0;
}

static int hwgtm_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int hwgtm_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long hwgtm_manager_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case HWGTM_SYS_EVENT:
		ret = hwgtm_system_event(arg);
		break;
	default:
		hwgtm_logd("unknown ioctl command: %d", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}


static long hwgtm_client_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case HWGTM_REQUEST_TASK:
		ret = hwgtm_request_task(arg);
		break;
	case HWGTM_START_TASK:
		ret = hwgtm_start_task(arg);
		break;
	case HWGTM_FINISH_TASK:
		ret = hwgtm_finish_task(arg);
		break;
	default:
		hwgtm_logd("unknown ioctl command: %d", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations g_hwgtm_manager_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= hwgtm_manager_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= hwgtm_manager_ioctl,
#endif
	.open		= hwgtm_open,
	.release	= hwgtm_close,
};

static const struct file_operations g_hwgtm_client_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= hwgtm_client_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= hwgtm_client_ioctl,
#endif
	.open		= hwgtm_open,
	.release	= hwgtm_close,
};

static struct miscdevice hwgtm_manager_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "hwgtm_manager",
	.fops	= &g_hwgtm_manager_fops,
};

static struct miscdevice hwgtm_client_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "hwgtm_client",
	.fops	= &g_hwgtm_client_fops,
};

static int __init hwgtm_init(void)
{
	int i;
	int j;
	int ret;
	hwgtm_logi("start");

	g_hwgtm_info = vmalloc(sizeof(struct hwgtm_info));
	if (!g_hwgtm_info) {
		ret = -ENOMEM;
		goto out1;
	}

	spin_lock_init(&g_hwgtm_info->gtm_lock);
	spin_lock(&g_hwgtm_info->gtm_lock);
	for (i = 0; i < HWGTM_SYS_EVENT_NUM; i++)
		g_hwgtm_info->events_last_time[i] = 0;

	for (i = 0; i < HWGTM_TASK_TYPE_NUM; i++)
		g_hwgtm_info->hwgtm_task_count[i] = 0;

	for (i = 0; i < HWGTM_TASK_TYPE_NUM; i++)
		for (j = 0; j < HWGTM_TASK_LIST_SIZE; j++)
			hwgtm_reset_task_locked(&g_hwgtm_info->hwgtm_task_container[i][j]);

	spin_unlock(&g_hwgtm_info->gtm_lock);

	ret = misc_register(&hwgtm_manager_miscdev);
	if (ret) {
		hwgtm_logi("failed to register hwgtm_manager");
		goto out2;
	}

	ret = misc_register(&hwgtm_client_miscdev);
	if (ret) {
		hwgtm_logi("failed to register hwgtm_client");
		goto out3;
	}

	goto out1;

out3:
	misc_deregister(&hwgtm_manager_miscdev);

out2:
	vfree(g_hwgtm_info);
	g_hwgtm_info = NULL;
out1:
	hwgtm_logi("end, ret=%d", ret);
	return ret;
}

static void __exit hwgtm_exit(void)
{
	if (g_hwgtm_info) {
		misc_deregister(&hwgtm_client_miscdev);
		misc_deregister(&hwgtm_manager_miscdev);
		vfree(g_hwgtm_info);
		g_hwgtm_info = NULL;
	}
}

module_init(hwgtm_init);
module_exit(hwgtm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Huawei global task manager");
MODULE_AUTHOR("Huawei Technologies Co., Ltd.");
