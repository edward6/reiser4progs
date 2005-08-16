/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.h -- repair backup method declaration. */

#ifndef REPAIR_BACKUP_H
#define REPAIR_BACKUP_H

#include <repair/repair.h>

extern errno_t repair_backup_pack(reiser4_fs_t *fs, 
				  aal_stream_t *stream);

extern errno_t repair_backup_unpack(reiser4_fs_t *fs, 
				    aal_stream_t *stream);

extern reiser4_backup_t *repair_backup_open(reiser4_fs_t *fs, 
					    uint8_t mode);

extern reiser4_backup_t *repair_backup_reopen(reiser4_fs_t *fs);
#endif
