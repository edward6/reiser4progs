/*
    repair/filter.h -- the structures and methods needed for fsck pass1. 
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#ifndef REPAIR_FILTER_H
#define REPAIR_FILTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_filter_pass(repair_data_t *);

#endif
