/*
    repair/scan.h -- common structures and methods for fsck pass2.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef REPAIR_SCAN_H
#define REPAIR_SCAN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_scan_node_check(reiser4_joint_t *, void *);

#endif
