/*
    repair/add_missing.h -- the common structures and methods for insertion leaves
    and extent item from twigs unconnected from the tree.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef ADD_MISSING_H
#define ADD_MISSING_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* Statistics gathered during the pass. */
typedef struct repair_am_info {
} repair_am_info_t;

/* Add missing. */
typedef struct repair_am {
    repair_data_t *repair;

    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;

    repair_am_info_t info;
} repair_am_t;

extern errno_t repair_add_missing(repair_am_t *am);

#endif

