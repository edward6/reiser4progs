/*  Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
    
    repair/format.h -- reiserfs filesystem recovery structures and
    macros. */

#ifndef REPAIR_FORMAT_H
#define REPAIR_FORMAT_H

#include <reiser4/filesystem.h>

extern errno_t repair_format_check_struct(reiser4_fs_t *fs, 
					  uint8_t mode);

extern errno_t repair_format_update(reiser4_format_t *format);

extern errno_t repair_format_pack(reiser4_format_t *format, 
				  aal_stream_t *stream);

extern reiser4_format_t *repair_format_unpack(reiser4_fs_t *fs,
					      aal_stream_t *stream);

extern void repair_format_print(reiser4_format_t *format, 
				aal_stream_t *stream);

extern errno_t repair_format_check_backup(aal_device_t *device, 
					  backup_hint_t *hint);

#endif
