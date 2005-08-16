/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_unix.h -- stat data exception plugin, that implements unix stat data
   fields. */

#ifndef SDEXT_UNIX_H
#define SDEXT_UNIX_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

typedef struct sdext_unix {
	d32_t uid;
	d32_t gid;
	d32_t atime;
	d32_t mtime;
	d32_t ctime;
	union {
		d64_t rdev;
		d64_t bytes;
	} u;
} __attribute__((packed)) sdext_unix_t;

extern reiser4_core_t *sdext_unix_core;

#define sdext_unix_get_uid(ext)		aal_get_le32(ext, uid)
#define sdext_unix_set_uid(ext, val)	aal_set_le32(ext, uid, val)

#define sdext_unix_get_gid(ext)		aal_get_le32(ext, gid)
#define sdext_unix_set_gid(ext, val)	aal_set_le32(ext, gid, val)

#define sdext_unix_get_atime(ext)	aal_get_le32(ext, atime)
#define sdext_unix_set_atime(ext, val)	aal_set_le32(ext, atime, val)

#define sdext_unix_get_mtime(ext)	aal_get_le32(ext, mtime)
#define sdext_unix_set_mtime(ext, val)	aal_set_le32(ext, mtime, val)

#define sdext_unix_get_ctime(ext)	aal_get_le32(ext, ctime)
#define sdext_unix_set_ctime(ext, val)	aal_set_le32(ext, ctime, val)

#define sdext_unix_get_rdev(ext)	        aal_get_le64(ext, u.rdev)
#define sdext_unix_set_rdev(ext, val)	aal_set_le64(ext, u.rdev, val)

#define sdext_unix_get_bytes(ext)	aal_get_le64(ext, u.bytes)
#define sdext_unix_set_bytes(ext, val)	aal_set_le64(ext, u.bytes, val)

#endif

