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

extern errno_t repair_master_check(reiser4_fs_t *fs, 
    callback_ask_user_t ask_blocksize);

#endif
