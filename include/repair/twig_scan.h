/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/twig_scan.h -- common structures and methods for the second
   fsck pass. */

#ifndef REPAIR_SCAN_H
#define REPAIR_SCAN_H

#include <time.h>
#include <repair/librepair.h>

/* Statistics gathered during the pass. */
typedef struct repair_ts_stat {
	uint64_t read_twigs, fixed_twigs;
	uint64_t bad_unfm_ptrs;
	time_t time;
} repair_ts_stat_t;


/* Twig scan data. */
typedef struct repair_ts {
	repair_data_t *repair;

	aux_bitmap_t *bm_used;		/* In the tree blocks .		     */
	aux_bitmap_t *bm_twig;		/* To be scanned blocks.	     */
	aux_bitmap_t *bm_met;		/* Met blocks, cannot be pointed by 
					   extents.			     */
	
	/* Results. */
	aux_bitmap_t *bm_unfm;		/* Unformatted blocks pointed from.  */   
	repair_progress_handler_t *progress_handler;    
	repair_progress_t *progress;
	repair_ts_stat_t stat;
} repair_ts_t;

extern errno_t repair_twig_scan(repair_ts_t *ts);

#endif
