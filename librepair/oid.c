/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid.c -- oid allocator repair code. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/libreiser4.h>

void repair_oid_print(reiser4_oid_t *oid, aal_stream_t *stream) {
	aal_assert("umka-1562", oid != NULL);
	aal_assert("umka-1563", stream != NULL);

	plug_call(oid->ent->plug->o.oid_ops,
		  print, oid->ent, stream, 0);
}

/* Returns lost+found object id from specified oid allocator */
oid_t repair_oid_lost_objectid(reiser4_oid_t *oid) {
	aal_assert("vpf-1552", oid != NULL);

	return plug_call(oid->ent->plug->o.oid_ops,
			 lost_objectid, oid->ent);
}

#endif
