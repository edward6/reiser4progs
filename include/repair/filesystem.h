/*  Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
    
    repair/filesystem.h -- reiserfs filesystem recovery structures 
    and macros. */

#ifndef REPAIR_FILESYSTEM_H
#define REPAIR_FILESYSTEM_H

#include <repair/repair.h>
#include <reiser4/filesystem.h>

extern errno_t repair_fs_open(repair_data_t *repair, 
			      aal_device_t *host_device,
			      aal_device_t *journal_device);

extern errno_t repair_fs_replay(reiser4_fs_t *fs);

extern errno_t repair_fs_pack(reiser4_fs_t *fs, 
			      aux_bitmap_t *bitmap, 
			      aal_stream_t *stream);

extern reiser4_fs_t *repair_fs_unpack(aal_device_t *device, 
				      aux_bitmap_t *bitmap,
				      aal_stream_t *stream);

extern errno_t repair_fs_lost_key(reiser4_fs_t *fs, 
				  reiser4_key_t *key);

extern errno_t repair_fs_check_backup(aal_device_t *device, 
				      backup_hint_t *hint);

extern errno_t repair_fs_block_pack(aal_block_t *block, 
				    aal_stream_t *stream);

extern aal_block_t *repair_fs_block_unpack(reiser4_fs_t *fs, 
					   aal_stream_t *stream);

#endif
