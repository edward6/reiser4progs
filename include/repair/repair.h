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
#include <repair/repair_plugin.h>

typedef struct repair_check_info {
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
} repair_check_info_t;

/*
#define repair_result(info, result)	\
{					\
    if (result == REPAIR_FATAL)		\
	(info)->fatal++;		\
    else if (result == REPAIR_FIXABLE)	\
	(info)->fixable++;		\
}
*/

typedef union repair_info {
    repair_check_info_t check;    
} repair_info_t;

/*
typedef struct repair_data {
    reiser4_fs_t *fs;
    repair_info_t info;
    uint8_t mode;
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
*/

/*
#define repair_set_flag(data, flag)	(aal_set_bit(&(data)->flags, flag))
#define repair_test_flag(data, flag)	(aal_test_bit(&(data)->flags, flag))
#define repair_clear_flag(data, flag)	(aal_clear_bit(&(data)->flags, flag))
*/

extern errno_t repair_check(reiser4_fs_t *fs, repair_info_t *info, uint8_t mode);

#endif

