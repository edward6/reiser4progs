/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid40_repair.h -- reiser4 oid allocator plugin repair functions. */

#ifndef OID40_REPAIR_H
#define OID40_REPAIR_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/plugin.h>

extern void oid40_print(generic_entity_t *entity,
			aal_stream_t *stream,
			uint16_t options);
#endif

#endif
