/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.h -- reiser4 backup method declaration. */

#ifndef REISER4_BACKUP_H
#define REISER4_BACKUP_H

#ifndef ENABLE_STAND_ALONE

extern errno_t reiser4_backup_layout(reiser4_fs_t *fs,
				     region_func_t region_func, 
				     void *data);

extern void reiser4_backup_sync(reiser4_backup_t *backup);
extern void reiser4_backup_close(reiser4_backup_t *backup);

extern reiser4_backup_t *reiser4_backup_create(reiser4_fs_t *fs);

#endif
#endif