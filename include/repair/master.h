/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/master.h -- reiserfs master superblock recovery structures
   and macros. */

#ifndef REPAIR_MASTER_H
#define REPAIR_MASTER_H

#include <repair/repair.h>

extern errno_t repair_master_open(reiser4_fs_t *fs, uint8_t mode);

extern errno_t repair_master_pack(reiser4_master_t *master,
				  aal_stream_t *stream);

extern reiser4_master_t *repair_master_unpack(aal_device_t *device,
					      aal_stream_t *stream);

extern void repair_master_print(reiser4_master_t *master,
				aal_stream_t *stream,
				uuid_unparse_t unparse);

#endif
