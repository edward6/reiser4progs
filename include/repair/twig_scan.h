/*
    repair/twig_scan.h -- common structures and methods for the second fsck pass.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef REPAIR_SCAN_H
#define REPAIR_SCAN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_ts_pass(repair_data_t *);

#endif
