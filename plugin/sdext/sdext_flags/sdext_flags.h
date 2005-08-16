/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_flags.h -- inode flags stat data extension plugin. */

#ifndef SDEXT_FLAGS_H
#define SDEXT_FLAGS_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

typedef struct sdext_flags {
	d32_t flags;
} __attribute__((packed)) sdext_flags_t;

extern reiser4_core_t *sdext_flags_core;

#define sdext_flags_get_flags(ext)	 aal_get_le32(ext, flags)
#define sdext_flags_set_flags(ext, val)  aal_set_le32(ext, flags, val)

#endif

