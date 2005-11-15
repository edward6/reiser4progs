/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid40_repair.h -- reiser4 oid allocator plugin repair functions. */

#ifndef OID40_REPAIR_H
#define OID40_REPAIR_H

#ifndef ENABLE_MINIMAL
#include <reiser4/plugin.h>

extern void oid40_print(reiser4_oid_ent_t *entity,
			aal_stream_t *stream,
			uint16_t options);

extern oid_t oid40_lost_objectid();
extern oid_t oid40_slink_locality();

#endif

#endif
