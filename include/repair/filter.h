/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/filter.h -- the structures and methods needed for fsck pass1. */

#ifndef REPAIR_FILTER_H
#define REPAIR_FILTER_H

#include <time.h>
#include <repair/librepair.h>

/* Statistics gathered during the pass. */
typedef struct repair_filter_stat {
	uint64_t read_nodes;
	uint64_t good_nodes,   good_leaves,   good_twigs;
	uint64_t fixed_nodes,  fixed_leaves,  fixed_twigs;
	uint64_t bad_nodes,    bad_leaves,    bad_twigs;
	uint64_t bad_dk_nodes, bad_dk_leaves, bad_dk_twigs;
	uint64_t bad_ptrs;
	time_t time;
} repair_filter_stat_t;

/* Data filter works on. */
typedef struct repair_filter {
	repair_data_t *repair;
    
	aux_bitmap_t *bm_used;	/* FS system area + formatted nodes. */
	
	/* Results of the work. */
	aux_bitmap_t *bm_leaf;	/* Bitmap of found leaves. */
	aux_bitmap_t *bm_twig;	/* Bitmap of found twigs. */
	aux_bitmap_t *bm_met;	/* Bitmap of formatted nodes which does not get
				   to neither other bitmap due to corruption or
				   just an internal one. */
	uint8_t level;
	uint64_t flags;

	repair_progress_handler_t *progress_handler;
	repair_progress_t *progress;
	repair_filter_stat_t stat;
	
	bool_t *check_node;
	uint64_t oid;
} repair_filter_t;

extern errno_t repair_filter(repair_filter_t *filter);

#endif
