/*
    repair/twig_scan.h -- common structures and methods for the second fsck pass.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_SCAN_H
#define REPAIR_SCAN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

/* Statistics gathered during the pass. */
typedef struct repair_ts_info {
} repair_ts_info_t;


/* Twig scan data. */
typedef struct repair_ts {
    repair_data_t *repair;

    aux_bitmap_t *bm_used;	/* Bitmap of blocks which are in the treee already. */
    aux_bitmap_t *bm_twig;	/* Bitmap of blocks should be scanned. */
    aux_bitmap_t *bm_met;	/* Bitmap of met blocks, cannot be pointed by extents. */
    /* Results. */
    aux_bitmap_t *bm_unfm_tree;	/* Unformatted blocks pointed from the tree. */
    aux_bitmap_t *bm_unfm_out;	/* Unformatted blocks pointed out of the tree. */
    
    repair_ts_info_t info;
} repair_ts_t;

extern errno_t repair_twig_scan(repair_ts_t *ts);

#endif
