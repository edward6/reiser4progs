/*
    repair/filter.h -- the structures and methods needed for fsck pass1. 

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_FILTER_H
#define REPAIR_FILTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* Statistics gathered during the pass. */
typedef struct repair_filter_info {
} repair_filter_info_t;

/* Data filter works on. */
typedef struct repair_filter {
    repair_data_t *repair;
    
    aux_bitmap_t *bm_used;	/* Formatted area + formatted nodes. */
    /* Results of the work. */
    aux_bitmap_t *bm_leaf;	/* Bitmap of found leaves. */
    aux_bitmap_t *bm_twig;	/* Bitmap of found twigs. */
    aux_bitmap_t *bm_met;	/* Bitmap of formatted nodes which cannot neither 
				   be pointed by extents nor marked nowhere else. */
    uint8_t level;
    uint8_t flags;

    repair_progress_handler_t *progress_handler;
    repair_progress_t *progress;
    repair_filter_info_t info;
} repair_filter_t;

extern errno_t repair_filter(repair_filter_t *filter);

#endif
