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

#include <repair/repair.h>

extern errno_t repair_filter_pass(repair_data_t *rd);
extern errno_t repair_filter_release(repair_data_t *rd);

#endif
