/*
    repair/repair.h -- the common structures and methods for recovery.
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#ifndef REPAIR_H
#define REPAIR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aux/bitmap.h>
#include <reiser4/reiser4.h>

/* Repair modes. */
#define REPAIR_CHECK	0x1
#define REPAIR_REBUILD	0x2
#define REPAIR_ROLLBACK	0x3

#define repair_mode(repair_data)	((repair_data)->mode)

/* Repair options. */
#define REPAIR_OPT_AUTO		0x2
#define REPAIR_OPT_FORCE	0x3
#define REPAIR_OPT_QUIET	0x4
#define REPAIR_OPT_VERBOSE	0x5
#define REPAIR_OPT_READ_ONLY	0x6

#define repair_set_option(bit, repair_data)	(aal_set_bit(&(repair_data)->options, bit))
#define repair_test_option(bit, repair_data)	(aal_test_bit(&(repair_data)->options, bit))
#define repair_clear_option(bit, repair_data)	(aal_clear_bit(&(repair_data)->options, bit))

#define repair_auto(repair_data)	(repair_test_option(REPAIR_OPT_AUTO, repair_data))
#define repair_force(repair_data)	(repair_test_option(REPAIR_OPT_FORCE, repair_data))
#define repair_quiet(repair_data)	(repair_test_option(REPAIR_OPT_QUIET, repair_data))
#define repair_verbose(repair_data)	(repair_test_option(REPAIR_OPT_VERBOSE, repair_data))
#define repair_read_only(repair_data)	(repair_test_option(REPAIR_OPT_READ_ONLY, repair_data))

/* Filter data. */
typedef struct repair_filter {
    aux_bitmap_t *bm_used;	/* Formatted area + formatted nodes. */
    aux_bitmap_t *bm_twig;	/* Twig nodes */
    
    uint8_t level;
} repair_filter_t;

/* Disk scan data. */
typedef struct repair_ds {
    aux_bitmap_t *bm_used;
    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;	/* Leaf bitmap not in the tree yet. */
    aux_bitmap_t *bm_frmt;	/* Formatted nodes bitmap cannot be pointed 
				   by extents, not marked nowhere else. */
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
    aux_bitmap_t *bm_used;
    aux_bitmap_t *bm_twig;
    aux_bitmap_t *bm_leaf;
    
    reiser4_tree_t *tree;

    reiser4_key_t max_real_key;
} repair_am_t;

typedef struct repair_data {
    reiser4_fs_t *fs;
    reiser4_profile_t *profile;
    uint16_t options;
    uint16_t mode;
    uint16_t flags;
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

/* Temporary flags set during recovery. */
#define REPAIR_BAD_PTR			0x1

#define repair_set_flag(data, flag)	(aal_set_bit(&(data)->flags, flag))
#define repair_test_flag(data, flag)	(aal_test_bit(&(data)->flags, flag))
#define repair_clear_flag(data, flag)	(aal_clear_bit(&(data)->flags, flag))

#endif

