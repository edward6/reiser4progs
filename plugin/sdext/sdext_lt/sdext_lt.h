/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt.h -- stat data plugin, that implements large time storage. */

#ifndef SDEXT_LT_H
#define SDEXT_LT_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct sdext_lt {
	d32_t atime;
	d32_t mtime;
	d32_t ctime;
} __attribute__((packed));

typedef struct sdext_lt sdext_lt_t;

#define sdext_lt_get_atime(lt)	        aal_get_le32(lt, atime)
#define sdext_lt_set_atime(lt, val)	aal_set_le32(lt, atime, val)

#define sdext_lt_get_mtime(lt)	        aal_get_le32(lt, mtime)
#define sdext_lt_set_mtime(lt, val)	aal_set_le32(lt, mtime, val)

#define sdext_lt_get_ctime(lt)	        aal_get_le32(lt, ctime)
#define sdext_lt_set_ctime(lt, val)	aal_set_le32(lt, ctime, val)

#endif

