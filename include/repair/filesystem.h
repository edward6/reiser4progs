/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
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

extern void repair_fs_close(reiser4_fs_t *fs);

extern errno_t repair_fs_pack(reiser4_fs_t *fs, 
			      aux_bitmap_t *bitmap, 
			      aal_stream_t *stream);

extern reiser4_fs_t *repair_fs_unpack(aal_device_t *device, 
				      aux_bitmap_t *bitmap,
				      aal_stream_t *stream);

#endif
