/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_unix.h -- stat data exception plugin, that implements unix stat data
   fields. */

#ifndef SDEXT_UNIX_H
#define SDEXT_UNIX_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct sdext_unix {
	d32_t uid;
	d32_t gid;
	d32_t atime;
	d32_t mtime;
	d32_t ctime;
	union {
		d64_t rdev;
		d64_t bytes;
	} u;
} __attribute__((packed));

typedef struct sdext_unix sdext_unix_t;

#define sdext_unix_get_uid(ux)		aal_get_le32(ux, uid)
#define sdext_unix_set_uid(ux, val)	aal_set_le32(ux, uid, val)

#define sdext_unix_get_gid(ux)		aal_get_le32(ux, gid)
#define sdext_unix_set_gid(ux, val)	aal_set_le32(ux, gid, val)

#define sdext_unix_get_atime(ux)	aal_get_le32(ux, atime)
#define sdext_unix_set_atime(ux, val)	aal_set_le32(ux, atime, val)

#define sdext_unix_get_mtime(ux)	aal_get_le32(ux, mtime)
#define sdext_unix_set_mtime(ux, val)	aal_set_le32(ux, mtime, val)

#define sdext_unix_get_ctime(ux)	aal_get_le32(ux, ctime)
#define sdext_unix_set_ctime(ux, val)	aal_set_le32(ux, ctime, val)

#define sdext_unix_get_rdev(ux)	        aal_get_le64(ux, u.rdev)
#define sdext_unix_set_rdev(ux, val)	aal_set_le64(ux, u.rdev, val)

#define sdext_unix_get_bytes(ux)	aal_get_le64(ux, u.bytes)
#define sdext_unix_set_bytes(ux, val)	aal_set_le64(ux, u.bytes, val)

#endif

