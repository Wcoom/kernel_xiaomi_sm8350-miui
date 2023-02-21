
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <log/hiview_hievent.h>
#include <log/hw_log.h>
#include <log/log_exception.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <securec.h>

#define DELAYED_TIME_MS 20000
#define OCP_DMD_CODE 920007000
#define REG_NUM 3
#define BUFF_SIZE 256

extern int huawei_pmic_reg_write(u8 sid, u16 addr, const u8 *buf, size_t len);
extern int huawei_pmic_reg_read(u8 sid, u16 addr, u8 *buf, size_t len);
static void ocp_handler(struct  work_struct *work);
static DECLARE_DELAYED_WORK(ocp_dmd_work, ocp_handler);
static char* info_table[] = { "SPMS", "LDO", "BOB" };

static void ocp_dmd_report(void)
{
    u8 buf_vreg[REG_NUM] = {0};
    char content_info[BUFF_SIZE] = {0};
    struct hiview_hievent *hi_event = NULL;
    int ret;
    huawei_pmic_reg_read(0, 0x9E54, buf_vreg, 1); // slave_id
    huawei_pmic_reg_read(0, 0x9E55, buf_vreg + 1, 1); // type
    huawei_pmic_reg_read(0, 0x9E56, buf_vreg + 2, 1); // index
    ret = snprintf_s(content_info, sizeof(content_info), sizeof(content_info) - 1, "OCP_PMIC%d_%s_%d", \
        buf_vreg[0], info_table[buf_vreg[1]], buf_vreg[2]);
    if (ret < 0) {
        printk("ocp_dmd_report: snprintf_s failed\n");
        return;
    }
    hi_event = hiview_hievent_create(OCP_DMD_CODE);
    hiview_hievent_put_string(hi_event, "CONTENT", content_info);
    hiview_hievent_report(hi_event);
    hiview_hievent_destroy(hi_event);
    // clear reg
    buf_vreg[0] = 0;
    buf_vreg[1] = 0;
    buf_vreg[2] = 0;
    huawei_pmic_reg_write(0, 0x9E54, buf_vreg, 3);
    return;
}

void ocp_handler(struct work_struct *work)
{
    u8 buf_vreg[REG_NUM] = {0};
    huawei_pmic_reg_read(0, 0x9E56, buf_vreg, 1);
    if (buf_vreg[0])
        ocp_dmd_report();
    return;
}



static int __init pmic_ocp_dmd_init(void)
{
    printk("pmic_ocp_dmd_init start.\n");
    schedule_delayed_work(&ocp_dmd_work, msecs_to_jiffies(DELAYED_TIME_MS));
    return 0;
}

static void __exit pmic_ocp_dmd_exit(void)
{
    printk("pmic_ocp_dmd_exit\n");
    return;
}
module_init(pmic_ocp_dmd_init);
module_exit(pmic_ocp_dmd_exit);