/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/master.h -- reiserfs master superblock recovery structures
   and macros. */

#ifndef REPAIR_MASTER_H
#define REPAIR_MASTER_H

#include <repair/repair.h>

extern errno_t repair_master_check_struct(reiser4_fs_t *fs, 
					  uint8_t mode,
					  uint32_t options);

extern errno_t repair_master_pack(reiser4_master_t *master,
				  aal_stream_t *stream);

extern reiser4_master_t *repair_master_unpack(aal_device_t *device,
					      aal_stream_t *stream);

extern void repair_master_print(reiser4_master_t *master,
				aal_stream_t *stream,
				uuid_unparse_t unparse);

extern errno_t repair_master_check_backup(backup_hint_t *hint);

#endif
