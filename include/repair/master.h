/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/master.h -- reiserfs master superblock recovery structures
   and macros. */

#ifndef REPAIR_MASTER_H
#define REPAIR_MASTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_master_open(reiser4_fs_t *fs, uint8_t mode);

#endif
