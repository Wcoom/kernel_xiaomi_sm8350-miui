

/* gpu fault dmd */
#define GPU_FAULT_DMD       922002000
#define GPU_PAGE_FAULT_DMD  922002001
#define GPU_SOFT_RESET_DMD  922002004
#define GPU_HARD_RESET_DMD  922002005

void report_gpu_dmd_inirq(int state, const char* context);