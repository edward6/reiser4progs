/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   repair/filter.h -- the structures and methods needed for lost&found pass of fsck. */

#ifndef REPAIR_LOST_FOUND_H
#define REPAIR_LOST_FOUNS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/librepair.h>
#include <time.h>

/* Statistics gathered during the pass. */
typedef struct repair_lost_found_stat {
	time_t time;
} repair_lost_found_stat_t;

/* Data filter works on. */
typedef struct repair_lost_found {
	repair_data_t *repair;
    	aal_list_t *path;
	
	repair_progress_handler_t *progress_handler;
	repair_progress_t *progress;
	reiser4_object_t *lost;
	repair_lost_found_stat_t stat;
} repair_lost_found_t;

extern errno_t repair_lost_found(repair_lost_found_t *lf);

#endif
