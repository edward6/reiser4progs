/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_flags.h -- inode flags stat data extension plugin. */

#ifndef SDEXT_FLAGS_H
#define SDEXT_FLAGS_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct sdext_flags {
	d32_t flags;
} __attribute__((packed));

typedef struct sdext_flags sdext_flags_t;

#define sdext_flags_get_flags(ext)	 aal_get_le32(ext, flags)
#define sdext_flags_set_flags(ext, val)  aal_set_le32(ext, flags, val)

#endif

