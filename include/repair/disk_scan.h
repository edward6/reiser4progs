/*
    repair/disk_scan.h -- the structures and methods needed for the second pass
    of fsck. 
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#ifndef REPAIR_DS_H
#define REPAIR_DS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_ds_pass(repair_data_t *);

#endif
