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

/* Data filter works on. */
typedef struct repair_filter {
    reiser4_fs_t *fs;
    aux_bitmap_t *bm_used;	/* Formatted area + formatted nodes. */

    /* Results of the work. */
    aux_bitmap_t *bm_leaf;	/* Bitmap of found leaves. */
    aux_bitmap_t *bm_twig;	/* Bitmap of found twigs. */
    aux_bitmap_t *bm_met;	/* Bitmap of formatted nodes which cannot neither 
				   be pointed by extents nor marked nowhere else. */
    repair_info_t info;
    uint8_t mode;
    uint8_t level;
    uint8_t flags;
} repair_filter_t;

extern errno_t repair_filter(repair_filter_t *filter);

#endif
