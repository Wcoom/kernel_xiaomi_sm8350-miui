/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM memcheck

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH platform/trace/hooks

#if !defined(_TRACE_MEMCHECK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MEMCHECK_H

#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(mm_set_buddy_track,
	TP_PROTO(struct page *p, unsigned int order, unsigned long caller),
	TP_ARGS(p, order, caller));

DECLARE_HOOK(mm_track_lslub_pages,
	TP_PROTO(struct page *p, unsigned int order, unsigned long caller, bool isAlloc),
	TP_ARGS(p, order, caller, isAlloc));

DECLARE_HOOK(mm_set_slub_alloc_track,
	TP_PROTO(unsigned long caller),
	TP_ARGS(caller));

DECLARE_HOOK(mm_set_slub_free_track,
	TP_PROTO(unsigned long caller),
	TP_ARGS(caller));

DECLARE_RESTRICTED_HOOK(mm_mem_stats_show,
	TP_PROTO(int unused),
	TP_ARGS(unused), 1);

DECLARE_RESTRICTED_HOOK(mm_vmalloc_detail_show,
	TP_PROTO(int unused),
	TP_ARGS(unused), 1);

DECLARE_HOOK(mm_set_skb_pages_zone_state,
	TP_PROTO(struct page *page, unsigned int order, bool isAdd),
	TP_ARGS(page, order, isAdd));

#endif /* _TRACE_MEMCHECK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
