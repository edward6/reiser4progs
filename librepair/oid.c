/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid.c -- oid allocator repair code. */

#ifndef ENABLE_MINIMAL
#include <reiser4/libreiser4.h>

void repair_oid_print(reiser4_oid_t *oid, aal_stream_t *stream) {
	aal_assert("umka-1562", oid != NULL);

	reiser4call(oid, print, stream, 0);
}

/* Returns lost+found object id from specified oid allocator */
oid_t repair_oid_lost_objectid(reiser4_oid_t *oid) {
	aal_assert("vpf-1552", oid != NULL);

	return reiser4call(oid, lost_objectid);
}

#endif
