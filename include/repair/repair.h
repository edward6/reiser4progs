/*
    repair/repair.h -- the common structures and methods for recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_H
#define REPAIR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aux/bitmap.h>
#include <reiser4/reiser4.h>
#include <repair/repair_plugins.h>

struct repair_check_info {
    /* amounts of different kinds of corruptions. */
    uint64_t fatal;
    uint64_t fixable;
    /* amounts of different kinds of nodes. */
    uint64_t leaves;
    uint64_t twigs;
    uint64_t branches;
    uint64_t unformatted;
    uint64_t zero_unformatted;
    uint64_t broken;
};

typedef struct repair_check_info repair_check_info_t;

union repair_info {
    struct repair_check_info check;    
};

typedef union repair_info repair_info_t;
    
/* Filter data. */
typedef struct repair_filter {
    aux_bitmap_t *bm_used;	/* Formatted area + formatted nodes. */
    aux_bitmap_t *bm_twig;	/* Twig nodes */
    
    uint8_t level;
    uint16_t flags;
} repair_filter_t;

/* Disk scan data. */
typedef struct repair_ds {
    aux_bitmap_t *bm_used;
    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;	/* Leaf bitmap not in the tree yet. */
    aux_bitmap_t *bm_frmt;	/* Bitmap of formatted nodes which cannot neither 
				   be pointed by extents nor marked nowhere else. */
    aux_bitmap_t *bm_scan;	/* Block bitmap to be scanned here. */
} repair_ds_t;

/* Twig scan data. */
typedef struct repair_ts {
    aux_bitmap_t *bm_used;
    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;
    aux_bitmap_t *bm_met;	/* frmt | used | leaf | twig | (after ts) unfm */
    aux_bitmap_t *bm_unfm_tree;	/* Unformatted blocks pointed from the tree. */
    aux_bitmap_t *bm_unfm_out;	/* Unformatted blocks pointed out of the tree. */
} repair_ts_t;

/* Add missing. */
typedef struct repair_am {
    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;
    
    reiser4_tree_t *tree;

    reiser4_key_t max_real_key;
} repair_am_t;

typedef struct repair_data {
    reiser4_fs_t *fs;
    reiser4_profile_t *profile;
    repair_info_t info;
    uint16_t options;
    union {
	repair_filter_t filter;
	repair_ds_t ds;
	repair_ts_t ts;
	repair_am_t am;
    } pass;
} repair_data_t;

#define repair_filter(data) (&(data)->pass.filter)
#define repair_ts(data)	    (&(data)->pass.ts)
#define repair_ds(data)	    (&(data)->pass.ds)
#define repair_am(data)	    (&(data)->pass.am)

/*
#define repair_set_flag(data, flag)	(aal_set_bit(&(data)->flags, flag))
#define repair_test_flag(data, flag)	(aal_test_bit(&(data)->flags, flag))
#define repair_clear_flag(data, flag)	(aal_clear_bit(&(data)->flags, flag))
*/
#endif

