/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.h -- reiser4 backup method declaration. */

#ifndef REISER4_BACKUP_H
#define REISER4_BACKUP_H

#ifndef ENABLE_MINIMAL

/* Put backup copies into the power of (3/2) block numbers. */
#define BACKUP_EXP_LAYOUT(blk)		((blk * 3) >> 1)

extern reiser4_backup_t *reiser4_backup_create(reiser4_fs_t *fs);

extern reiser4_backup_t *reiser4_backup_open(reiser4_fs_t *fs);

extern void reiser4_backup_close(reiser4_backup_t *backup);

extern errno_t reiser4_backup_sync(reiser4_backup_t *backup);

extern errno_t reiser4_backup_valid(reiser4_backup_t *backup);

extern errno_t reiser4_backup_layout(reiser4_fs_t *fs,
				     region_func_t region_func, 
				     void *data);

extern errno_t reiser4_old_backup_layout(reiser4_fs_t *fs, 
					 region_func_t region_func,
					 void *data);

#define reiser4_backup_isdirty(backup) ((backup)->dirty)
#define reiser4_backup_mkdirty(backup) ((backup)->dirty = 1)
#define reiser4_backup_mkclean(backup) ((backup)->dirty = 0)

#endif
#endif
