/*
    repair/disk_scan.h -- the structures and methods needed for the second pass
    of fsck. 

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_DS_H
#define REPAIR_DS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

/* Statistics gathered during the pass. */
typedef struct repair_ds_info {
} repair_ds_info_t;

/* Disk scan data. */
typedef struct repair_ds {
    repair_data_t *repair;
    
    aux_bitmap_t *bm_scan;	/* Bitmap of blocks to be scanned on the pass. */
    aux_bitmap_t *bm_met;	/* Bitmap of blocks met already. 
				   Mark all formatted block here also. */
    /* Results of the work. */
    aux_bitmap_t *bm_leaf;	/* Found leaves. */
    aux_bitmap_t *bm_twig;	/* Fount twigs. */

    repair_progress_handler_t *progress_handler;    
    repair_ds_info_t info;
} repair_ds_t;

extern errno_t repair_disk_scan(repair_ds_t *ds);

#endif
