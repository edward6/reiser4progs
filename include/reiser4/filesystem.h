/*
  filesystem.h -- reiser4 filesystem functions declaration.    

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_FILESYSTEM_H
#define REISER4_FILESYSTEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

#ifndef ENABLE_COMPACT

extern reiser4_fs_t *reiser4_fs_create(aal_device_t *host_device,
				       char *uuid, char *label,
				       count_t len,
				       reiser4_profile_t *profile,
				       aal_device_t *journal_device, 
				       void *journal_hint);

extern errno_t reiser4_fs_clobber(aal_device_t *device);
extern errno_t reiser4_fs_sync(reiser4_fs_t *fs);
extern errno_t reiser4_fs_mark(reiser4_fs_t *fs);

#endif

extern reiser4_fs_t *reiser4_fs_open(aal_device_t *host_device, 
				     aal_device_t *journal_device);

extern errno_t reiser4_fs_layout(reiser4_fs_t *fs, block_func_t func, 
				 void *data);

extern void reiser4_fs_close(reiser4_fs_t *fs);
extern aal_device_t *reiser4_fs_host_device(reiser4_fs_t *fs);
extern aal_device_t *reiser4_fs_journal_device(reiser4_fs_t *fs);
extern reiser4_owner_t reiser4_fs_belongs(reiser4_fs_t *fs, blk_t blk);

#endif

