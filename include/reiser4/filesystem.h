/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   filesystem.h -- reiser4 filesystem functions declaration. */

#ifndef REISER4_FILESYSTEM_H
#define REISER4_FILESYSTEM_H

#include <reiser4/types.h>

extern void reiser4_fs_close(reiser4_fs_t *fs);

#ifndef ENABLE_MINIMAL
extern reiser4_fs_t *reiser4_fs_open(aal_device_t *device, bool_t check);
#else
extern reiser4_fs_t *reiser4_fs_open(aal_device_t *device);
#endif

#ifndef ENABLE_MINIMAL

#define FS_LEN_ADJUST (64 * 1024)

extern errno_t reiser4_fs_sync(reiser4_fs_t *fs);

extern errno_t reiser4_fs_clobber(aal_device_t *device);

extern errno_t reiser4_fs_resize(reiser4_fs_t *fs, count_t blocks);

extern errno_t reiser4_fs_copy(reiser4_fs_t *src_fs, reiser4_fs_t *dst_fs);

extern errno_t reiser4_fs_layout(reiser4_fs_t *fs,
				 region_func_t region_func, 
				 void *data);

extern reiser4_owner_t reiser4_fs_belongs(reiser4_fs_t *fs, blk_t blk);

extern reiser4_fs_t *reiser4_fs_create(aal_device_t *device,
				       fs_hint_t *hint);

extern errno_t reiser4_fs_backup(reiser4_fs_t *fs, backup_hint_t *hint);

#endif

#endif
