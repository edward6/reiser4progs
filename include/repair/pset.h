/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   pset.h -- repair plugin set (&heir set) method declarations. */

#ifndef REPAIR_PSET_H
#define REPAIR_PSET_H

#include <repair/repair.h>

extern errno_t repair_pset_check_backup(backup_hint_t *hint);

extern errno_t repair_pset_root_check(reiser4_fs_t *fs, 
				      reiser4_object_t *root, 
				      uint8_t mode);

#endif
