/*
    repair/twig_scan.h -- common structures and methods for the second fsck pass.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_SCAN_H
#define REPAIR_SCAN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_twig_scan_pass(repair_data_t *rd);

#endif
