/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lw.h -- stat data plugin, that implements base stat data fields. */

#ifndef SDEXT_LW_H
#define SDEXT_LW_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct sdext_lw {
	d16_t mode;
	d32_t nlink;
	d64_t size;
} __attribute__((packed));

typedef struct sdext_lw sdext_lw_t;

#define sdext_lw_get_mode(ext)		aal_get_le16(ext, mode)
#define sdext_lw_set_mode(ext, val)	aal_set_le16(ext, mode, val)

#define sdext_lw_get_nlink(ext)	        aal_get_le32(ext, nlink)
#define sdext_lw_set_nlink(ext, val)	aal_set_le32(ext, nlink, val)

#define sdext_lw_get_size(ext)		aal_get_le64(ext, size)
#define sdext_lw_set_size(ext, val)	aal_set_le64(ext, size, val)

#endif

