/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/disk_scan.h -- the structures and methods needed for the 
   second pass of fsck. */

#ifndef REPAIR_DS_H
#define REPAIR_DS_H

#include <time.h>
#include <repair/librepair.h>

/* Statistics gathered during the pass. */
typedef struct repair_ds_stat {
	uint64_t read_nodes;
	uint64_t good_nodes, good_leaves, good_twigs;
	uint64_t fixed_nodes, fixed_leaves, fixed_twigs;
	time_t time;
} repair_ds_stat_t;

/* Disk scan data. */
typedef struct repair_ds {
	repair_data_t *repair;
    
	aux_bitmap_t *bm_scan;	/* Blocks to be scanned on the pass. */
	aux_bitmap_t *bm_met;	/* Blocks met already + all formatted . */
	/* Results of the work. */
	aux_bitmap_t *bm_leaf;	/* Found leaves. */
	aux_bitmap_t *bm_twig;	/* Fount twigs. */

	repair_progress_handler_t *progress_handler;    
	repair_progress_t *progress;
	repair_ds_stat_t stat;

	bool_t *check_node;
} repair_ds_t;

extern errno_t repair_disk_scan(repair_ds_t *ds);

#endif
