/*
    repair/filesystem.h -- reiserfs filesystem recovery structures and macros.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_FILESYSTEM_H
#define REPAIR_FILESYSTEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>
#include <reiser4/filesystem.h>

extern reiser4_fs_t *repair_fs_open(aal_device_t *host_device, 
    reiser4_profile_t *profile);
extern errno_t repair_fs_sync(reiser4_fs_t *fs);
extern void repair_fs_close(reiser4_fs_t *fs);
extern errno_t repair_fs_check(reiser4_fs_t *fs, repair_data_t *repair_data);

#endif
