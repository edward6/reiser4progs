/*
    repair/master.h -- reiserfs master superblock recovery structures and macros.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef REPAIR_MASTER_H
#define REPAIR_MASTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_master_open(reiser4_fs_t *fs);

#endif
