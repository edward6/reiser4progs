/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid.c -- oid allocator repair code. */

#ifndef REPAIR_OID_H
#define REPAIR_OID_H

#ifndef ENABLE_STAND_ALONE
#include <repair/repair.h>

extern void repair_oid_print(reiser4_oid_t *oid, 
			     aal_stream_t *stream);

extern oid_t repair_oid_lost_objectid(reiser4_oid_t *oid);

#endif
#endif
