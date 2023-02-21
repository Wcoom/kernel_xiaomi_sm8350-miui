/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM memcheck

#if !defined(_TRACE_MEMCHECK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MEMCHECK_H

#include <linux/tracepoint.h>

TRACE_EVENT(mm_set_buddy_track,

	TP_PROTO(struct page *p, unsigned int order, unsigned long caller),

	TP_ARGS(p, order, caller),

	TP_STRUCT__entry(
		__field(struct page *, p)
		__field(unsigned int, order)
		__field(unsigned long, caller)
	),

	TP_fast_assign(
		__entry->p = p;
		__entry->order = order;
		__entry->caller = caller;
	),

	TP_printk("mm_set_buddy_track order=%d", __entry->order)
);

TRACE_EVENT(mm_track_lslub_pages,

	TP_PROTO(struct page *p, unsigned int order, unsigned long caller, bool isAlloc),

	TP_ARGS(p, order, caller, isAlloc),

	TP_STRUCT__entry(
		__field(struct page *, p)
		__field(unsigned int, order)
		__field(unsigned long, caller)
		__field(bool, isAlloc)
	),

	TP_fast_assign(
		__entry->p = p;
		__entry->order = order;
		__entry->caller = caller;
		__entry->isAlloc = isAlloc;
	),

	TP_printk("mm_track_lslub_pages")
);

TRACE_EVENT(mm_set_slub_alloc_track,

	TP_PROTO(unsigned long caller),

	TP_ARGS(caller),

	TP_STRUCT__entry(
		__field(unsigned long, caller)
	),

	TP_fast_assign(
		 __entry->caller = caller;
	),

	TP_printk("mm_set_slub_alloc_track")
);

TRACE_EVENT(mm_set_slub_free_track,

	TP_PROTO(unsigned long caller),

	TP_ARGS(caller),

	TP_STRUCT__entry(
		__field(unsigned long, caller)
	),

	TP_fast_assign(
		__entry->caller = caller;
	),

	TP_printk("mm_set_slub_free_track")
);

TRACE_EVENT(mm_mem_stats_show,

	TP_PROTO(int unused),

	TP_ARGS(unused),

	TP_STRUCT__entry(
	),

	TP_fast_assign(
	),

	TP_printk("mm_mem_stats_show")
);

TRACE_EVENT(mm_vmalloc_detail_show,

	TP_PROTO(int unused),

	TP_ARGS(unused),

	TP_STRUCT__entry(
	),

	TP_fast_assign(
	),

	TP_printk("mm_vmalloc_detail_show")
);

TRACE_EVENT(mm_set_skb_pages_zone_state,

	TP_PROTO(struct page *page, unsigned int order, bool isAdd),

	TP_ARGS(page, order, isAdd),

	TP_STRUCT__entry(
		__field(struct page *, page)
		__field(unsigned int, order)
		__field(bool, isAdd)
	),

	TP_fast_assign(
		__entry->page = page;
		__entry->order = order;
		__entry->isAdd = isAdd;
	),

	TP_printk("mm_set_skb_pages_zone_state")
);
#endif /* _TRACE_MEMCHECK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>