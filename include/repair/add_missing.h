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

/* Add missing. */
typedef struct repair_am {
    reiser4_fs_t *fs;
    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;

    repair_info_t info;
    uint8_t mode;
} repair_am_t;

extern errno_t repair_add_missing(repair_am_t *am);

#endif

