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

#define repair_set_option(bit, repair_data)	(aal_set_bit(bit, &(repair_data)->options))
#define repair_test_option(bit, repair_data)	(aal_test_bit(bit, &(repair_data)->options))
#define repair_clear_option(bit, repair_data)	(aal_clear_bit(bit, &(repair_data)->options))

#define repair_auto(repair_data)	(repair_test_option(REPAIR_OPT_AUTO, repair_data))
#define repair_force(repair_data)	(repair_test_option(REPAIR_OPT_FORCE, repair_data))
#define repair_quiet(repair_data)	(repair_test_option(REPAIR_OPT_QUIET, repair_data))
#define repair_verbose(repair_data)	(repair_test_option(REPAIR_OPT_VERBOSE, repair_data))
#define repair_read_only(repair_data)	(repair_test_option(REPAIR_OPT_READ_ONLY, repair_data))

typedef struct repair_filter_data {
    aux_bitmap_t *bm_formatted;
    uint8_t level;
} repair_filter_data_t;

typedef struct repair_scan_data {
    aux_bitmap_t *bm_used;
    reiser4_oid_t *oid_control;
} repair_scan_data_t;

typedef struct repair_check {
    reiser4_format_t *format;
    aux_bitmap_t *bm_format_layout;
    uint16_t options;
    uint16_t mode;
    uint16_t flags;  /*  */
    union {
	repair_filter_data_t filter;
	repair_scan_data_t scan;
    } pass;
} repair_data_t;

#define repair_filter_data(data)	(&(data)->pass.filter)
#define repair_scan_data(data)		(&(data)->pass.scan)

/* Temporary flags set during recovery. */
#define REPAIR_NOT_FIXED		0x1
#define REPAIR_BAD_PTR			0x2

#define repair_set_flag(data, flag)	(aal_set_bit(flag, &(data)->flags))
#define repair_test_flag(data, flag)	(aal_test_bit(flag, &(data)->flags))
#define repair_clear_flag(data, flag)	(aal_clear_bit(flag, &(data)->flags))

#endif

