/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   meta.h -- reiser4 metadata fetch/load stuff. */

#ifndef META_H
#define META_H

#include "types.h"

extern errno_t debugfs_pack_meta(reiser4_fs_t *fs,
				 char *filename);

extern reiser4_fs_t *debugfs_unpack_meta(aal_device_t *device,
					 char *filename);
#endif
