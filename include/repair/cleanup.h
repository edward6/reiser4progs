/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/cleanup.h -- the structures and methods needed for tree
   cleanuping as the end of filsystem recovery. */

#ifndef REPAIR_CLEANUP_H
#define REPAIR_CLEANUP_H

#include <time.h>
#include <repair/librepair.h>

/* Statistics gathered during the pass. */
typedef struct repair_cleanup_stat {
	uint64_t removed, linked;
	time_t time;
} repair_cleanup_stat_t;

/* Data filter works on. */
typedef struct repair_cleanup {
	repair_data_t *repair;
	
	repair_progress_handler_t *progress_handler;
	repair_progress_t *progress;
	repair_cleanup_stat_t stat;
} repair_cleanup_t;

extern errno_t repair_cleanup(repair_cleanup_t *cleanup);

#endif
