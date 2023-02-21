

#include <log/hiview_hievent.h>
#include <log/hw_log.h>
#include <log/log_exception.h>
#include "adreno.h"
#include <linux/workqueue.h>

#define MAX_CONTEXT_SIZE 256
static void gpu_report_worker(struct work_struct *work);
static DECLARE_WORK(gpu_dmd_work, gpu_report_worker);
int g_state = 0;
const char* g_context;

static void gpu_fault_dmd_report(int state, const char* context)
{
	int dmd_code;
	struct hiview_hievent *hi_event = NULL;
	switch (state) {
	case ADRENO_SOFT_FAULT: {
		dmd_code = GPU_SOFT_RESET_DMD;
		break;
	}
	case ADRENO_HARD_FAULT: {
		dmd_code = GPU_HARD_RESET_DMD;
		break;
	}
	case ADRENO_IOMMU_PAGE_FAULT: {
		dmd_code = GPU_PAGE_FAULT_DMD;
		break;
	}
	default:
		dmd_code = GPU_FAULT_DMD;
		break;
	}

	hi_event = hiview_hievent_create(dmd_code);
	hiview_hievent_put_string(hi_event, "CONTENT", context);
	hiview_hievent_report(hi_event);
	hiview_hievent_destroy(hi_event);
	return;
}

static void gpu_report_worker(struct work_struct *work)
{
	gpu_fault_dmd_report(g_state, g_context);
	return;
}

void report_gpu_dmd_inirq(int state, const char* context)
{
	g_state = state;
	g_context = context;
	schedule_work(&gpu_dmd_work);
	return;
}
