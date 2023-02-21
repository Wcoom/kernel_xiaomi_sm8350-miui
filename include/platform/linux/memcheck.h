#ifndef _MEMCHECK_H
#define _MEMCHECK_H

#include <linux/types.h>

#ifdef __KERNEL__
#ifdef CONFIG_DFX_MEMCHECK
#include <asm/ioctls.h>
#endif
#else /* __KERNEL__ */
#include <sys/types.h>
#include <sys/ioctl.h>
#endif /* __KERNEL__ */

#define MEMCHECK_CMD_INVALID	0xFFFFFFFF

#ifdef __KERNEL__
#ifdef CONFIG_DFX_MEMCHECK
struct file;
long memcheck_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else /* CONFIG_DFX_MEMCHECK */
static inline long memcheck_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	return MEMCHECK_CMD_INVALID;
}
#endif /* CONFIG_DFX_MEMCHECK */
#endif /* __KERNEL__ */

#ifdef CONFIG_DFX_MEMCHECK
#define SLAB_MM_NOTRACE     0x10000000UL
#define SLAB_MM_TRACE       0x20000000UL
void mm_mem_stats_show(void);
void mm_set_vmalloc_page_zone_state(struct page *page, bool is_add);
#else
#define SLAB_MM_NOTRACE     0x00000000UL
#define SLAB_MM_TRACE       0x00000000UL
static inline void mm_mem_stats_show(void)
{
}
static inline void mm_set_vmalloc_page_zone_state(struct page *page, bool is_add)
{
}
#endif

#ifdef CONFIG_DFX_MEMCHECK_DETAIL
void mm_vmalloc_detail_show(void);
#else
static inline void mm_vmalloc_detail_show(void)
{
}
#endif

#ifdef CONFIG_DFX_MEMCHECK_DETAIL
void get_slub_detail_info(void);
#else
static inline void get_slub_detail_info(void)
{
}
#endif

#ifdef CONFIG_DFX_MEMCHECK_STACK
int mm_buddy_track_map(int nid);
#else
static inline int mm_buddy_track_map(int nid)
{
	return 0;
}
#endif

#ifdef CONFIG_DFX_MEMCHECK_EXT
void mm_ion_process_info(void *idev);
#else /* CONFIG_DFX_MEMCHECK_EXT */
static inline void mm_ion_process_info(void *idev)
{
}
#endif /* CONFIG_DFX_MEMCHECK_EXT */

#ifdef CONFIG_DFX_MEMCHECK_EXT
void mm_ashmem_process_info(void);
#else /* CONFIG_DFX_MEMCHECK_EXT */
static inline void mm_ashmem_process_info(void)
{
}
#endif /* CONFIG_DFX_MEMCHECK_EXT */

#ifdef CONFIG_DFX_MEMCHECK_EXT
void ion_heap_show(void);
#else
static inline void ion_heap_show(void)
{
}
#endif /* CONFIG_DFX_MEMCHECK_EXT*/

#ifdef CONFIG_DFX_MEMCHECK_EXT
void ashmem_info_show(void);
#else
static inline void ashmem_info_show(void)
{
}
#endif /* CONFIG_DFX_MEMCHECK_EXT*/
#endif /* _MEMCHECK_H */
