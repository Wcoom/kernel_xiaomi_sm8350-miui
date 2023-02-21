
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <log/hiview_hievent.h>
#include <log/hw_log.h>
#include <log/log_exception.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <securec.h>

#include <linux/mtd/hw_nve_interface.h>

#define DELAYED_TIME_MS 60000


#define BUFF_SIZE 256
#define NVE_NV_DATA_SIZE 104

extern int hw_nve_direct_access(struct hw_nve_info_user *user_info);
static void autofix_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(autofix_dmd_work, autofix_handler);

#define AUTO_FIX_NV_ID 282
#define DMD_REPORT_NV_ID 498
#define AUTO_FIX_BIT 28  //第28位表示是否提压
#define AUTO_FIX_DMD 920007009
#define OK 0
#define ERROR -1

static int upload_dmd(unsigned int event_id, const char* event_content)
{
    int ret;
    struct hiview_hievent *hi_event = NULL;
    hi_event = hiview_hievent_create(event_id);
    if (!hi_event) {
        pr_err("%s: create hiview fail\n", __func__);
        return ERROR;
    }
    ret = hiview_hievent_put_string(hi_event, "CONTENT", event_content);
    if (ret < 0)
        pr_err("%s hiview put string fail.\n", __func__);
    ret = hiview_hievent_report(hi_event);
    if (ret < 0) {
        pr_err("%s: report hiview fail\n", __func__);
        return ERROR;
    }
    hiview_hievent_destroy(hi_event);
    return OK;
}

static int is_fixed(void) // 0: no need; 0:error; 1:need
{
    int check_bit = 0;
    int i = 0;
    int ret;
    struct hw_nve_info_user info_user = {0};
    info_user.nv_operation = NV_READ;
    info_user.nv_number = AUTO_FIX_NV_ID;
    info_user.valid_size = NVE_NV_DATA_SIZE;

    ret = hw_nve_direct_access(&info_user);
    if (ret) {
        pr_err("nve read failed\n");
        return 0;
    }
    if (info_user.nv_data[0] < '0' || info_user.nv_data[0] > '9') { // 首字符非数字，NV未被初始化
        return 0;
    } else {
        // check bit
        while (check_bit <= AUTO_FIX_BIT && i < NVE_NV_DATA_SIZE) {
            if (info_user.nv_data[i] == ';')
                check_bit++;
            i++;
        }
        if (check_bit != AUTO_FIX_BIT + 1)
            return 0;
        else if (info_user.nv_data[i - 2] == '1') {// trace back 2 bytes
            pr_info("the phone is fixed\n");
            return 1;
        } else {
            pr_info("the phone is not fixed: %c\n", info_user.nv_data[i - 2]);
            return 0;
        }
    }
}

static void fixed_report_write(void)
{
    int ret;
    struct hw_nve_info_user info_user = {0};
    info_user.nv_operation = NV_WRITE;
    info_user.nv_number = DMD_REPORT_NV_ID;
    info_user.valid_size = NVE_NV_DATA_SIZE;
    info_user.nv_data[0] = '1';
    ret = hw_nve_direct_access(&info_user);
    if (ret)
        pr_err("%s nve write failed\n", __func__);
    return;
}

static int is_reported(void)
{
    int ret;
    struct hw_nve_info_user info_user = {0};
    info_user.nv_operation = NV_READ;
    info_user.nv_number = DMD_REPORT_NV_ID;
    info_user.valid_size = NVE_NV_DATA_SIZE;
    ret = hw_nve_direct_access(&info_user);
    if (ret) {
        pr_err("is_reported nve read failed\n");
        return -1;
    }
    pr_info("is_reported check is_reported nvme %s\n", info_user.nv_data);

    if (info_user.nv_data[0] == '1') {
        pr_info("is_reported: is report\n");
        return 1;
    } else {
        pr_info("is_reported: is not report\n");
        return 0;
    }
}

static void fixed_handler(void)
{
    int ret;
    if (is_fixed() && !is_reported()) {
        pr_info("The phone is selffixed, report dmd.\n");
        ret = upload_dmd(AUTO_FIX_DMD, "the phone is selffixed");
        if (ret == OK) {
            pr_info("fixed_handler upload_dmd suss\n");
            fixed_report_write();
        }
    }
    return;
}


static void autofix_handler(struct work_struct *work)
{
    fixed_handler();
    return;
}

static int __init self_fix_check_init(void)
{
    pr_info("%s start.\n", __func__);
    schedule_delayed_work(&autofix_dmd_work, msecs_to_jiffies(DELAYED_TIME_MS));
    return 0;
}

static void __exit self_fix_check_exit(void)
{
    pr_info("self_fix_check_exit\n");
    return;
}
module_init(self_fix_check_init);
module_exit(self_fix_check_exit);
