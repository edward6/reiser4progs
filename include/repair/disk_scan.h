/*
    repair/disk_scan.h -- the structures and methods needed for the second pass
    of fsck. 

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_DS_H
#define REPAIR_DS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_disk_scan_pass(repair_data_t *rd);

#endif
