/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid40.h -- reiser4 default oid allocator structures. */

#ifndef OID40_H
#define OID40_H

#include <reiser4/plugin.h>

#define OID40_ROOT_LOCALITY	0x29
#define OID40_ROOT_OBJECTID	(OID40_ROOT_LOCALITY + 1)
#define OID40_SAFE_LOCALITY	(OID40_ROOT_OBJECTID + 1)

#define OID40_RESERVED		(1 << 16)

#define OID40_LOST_OBJECTID	(OID40_RESERVED - 1)

typedef struct oid40 {
	reiser4_oid_plug_t *plug;
	reiser4_format_ent_t *format;

	void *start;
	uint32_t len;
	
	uint64_t next;
	uint64_t used;
	uint32_t state;
} oid40_t;

extern reiser4_oid_plug_t oid40_plug;

#define PLUG_ENT(p) ((oid40_t *)p)

#define oid40_get_next(area)            LE64_TO_CPU(*((d64_t *)area))
#define oid40_set_next(area, val)       (*((d64_t *)area) = CPU_TO_LE64(val))

#define oid40_get_used(area)            LE64_TO_CPU(*(((d64_t *)area) + 1))
#define oid40_set_used(area, val)       (*(((d64_t *)area) + 1) = CPU_TO_LE64(val))

#endif

