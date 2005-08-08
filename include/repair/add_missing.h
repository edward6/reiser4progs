/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/add_missing.h -- the common structures and methods for insertion
   leaves and extent item from twigs unconnected from the tree. */

#ifndef ADD_MISSING_H
#define ADD_MISSING_H

#include <time.h>
#include <repair/librepair.h>

/* Statistics gathered during the pass. */
typedef struct repair_am_stat {
	uint64_t read_leaves, by_leaf, by_item_leaves;
	uint64_t read_twigs,  by_twig, by_item_twigs, empty;

	uint64_t *files;
	time_t time;
} repair_am_stat_t;

/* Add missing. */
typedef struct repair_am {
	repair_data_t *repair;
	
	reiser4_bitmap_t *bm_used;
	reiser4_bitmap_t *bm_twig;
	reiser4_bitmap_t *bm_leaf;
	
	repair_am_stat_t stat;
	aal_gauge_t *gauge;
} repair_am_t;

extern errno_t repair_add_missing(repair_am_t *am);

#endif
