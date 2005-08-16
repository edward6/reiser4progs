/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt.h -- stat data plugin, that implements large time storage. */

#ifndef SDEXT_LT_H
#define SDEXT_LT_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

typedef struct sdext_lt {
	d32_t atime;
	d32_t mtime;
	d32_t ctime;
} __attribute__((packed)) sdext_lt_t;

extern reiser4_core_t *sdext_lt_core;

#define sdext_lt_get_atime(ext)	        aal_get_le32(ext, atime)
#define sdext_lt_set_atime(ext, val)	aal_set_le32(ext, atime, val)

#define sdext_lt_get_mtime(ext)	        aal_get_le32(ext, mtime)
#define sdext_lt_set_mtime(ext, val)	aal_set_le32(ext, mtime, val)

#define sdext_lt_get_ctime(ext)	        aal_get_le32(ext, ctime)
#define sdext_lt_set_ctime(ext, val)	aal_set_le32(ext, ctime, val)

#endif

