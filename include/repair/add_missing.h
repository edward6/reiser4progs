/* repair/add_missing.h -- the common structures and methods for insertion leaves
   and extent item from twigs unconnected from the tree.
   
   Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING. */

#ifndef ADD_MISSING_H
#define ADD_MISSING_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/librepair.h>
#include <time.h>

/* Statistics gathered during the pass. */
typedef struct repair_am_stat {
	uint64_t read_leaves, by_leaf, by_item_leaves;
	uint64_t read_twigs,  by_twig, by_item_twigs;
	time_t time;
} repair_am_stat_t;

/* Add missing. */
typedef struct repair_am {
	repair_data_t *repair;
	
	aux_bitmap_t *bm_twig;
	aux_bitmap_t *bm_leaf;
	
	repair_progress_handler_t *progress_handler;    
	repair_progress_t *progress;
	repair_am_stat_t stat;
} repair_am_t;

extern errno_t repair_add_missing(repair_am_t *am);

#endif

