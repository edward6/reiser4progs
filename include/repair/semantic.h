/*
    repair/semantic.h -- the structures and methods needed for semantic 
    pass of fsck.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_SEM_H
#define REPAIR_SEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/librepair.h>
#include <time.h>

/* Statistics gathered during the pass. */
typedef struct repair_semantic_stat {
    time_t time;
} repair_semantic_stat_t;

/* Data filter works on. */
typedef struct repair_semantic {
    repair_data_t *repair;
    
    repair_progress_handler_t *progress_handler;
    repair_progress_t *progress;
    repair_semantic_stat_t stat;
} repair_semantic_t;

extern errno_t repair_semantic(repair_semantic_t *sem);

#endif

