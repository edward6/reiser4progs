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

#define sdext_lw_get_mode(lw)		aal_get_le16(lw, mode)
#define sdext_lw_set_mode(lw, val)	aal_set_le16(lw, mode, val)

#define sdext_lw_get_nlink(lw)	        aal_get_le32(lw, nlink)
#define sdext_lw_set_nlink(lw, val)	aal_set_le32(lw, nlink, val)

#define sdext_lw_get_size(lw)		aal_get_le64(lw, size)
#define sdext_lw_set_size(lw, val)	aal_set_le64(lw, size, val)

#endif

