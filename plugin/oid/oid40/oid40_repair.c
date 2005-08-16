/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid40_repair.c -- reiser4 oid allocator plugin. */

#ifndef ENABLE_MINIMAL
#include "oid40.h"

/* Prints oid allocator data into passed @stream */
void oid40_print(generic_entity_t *entity, 
		 aal_stream_t *stream, 
		 uint16_t options)
{
	aal_assert("umka-1303", entity != NULL);
	aal_assert("umka-1304", stream != NULL);

	aal_stream_format(stream, "Oid allocator:\n");
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  entity->plug->label);

	aal_stream_format(stream, "next oid:\t0x%llx\n",
			  ((oid40_t *)entity)->next);

	aal_stream_format(stream, "used oids:\t%llu\n",
			  ((oid40_t *)entity)->used);
}

oid_t oid40_lost_objectid() {
	return OID40_LOST_OBJECTID;
}

oid_t oid40_slink_locality() {
	return OID40_SAFE_LOCALITY;
}

#endif
