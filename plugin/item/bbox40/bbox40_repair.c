/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.c -- reiser4 default tail plugin. */


#ifndef ENABLE_STAND_ALONE
#include <repair/plugin.h>
#include "bbox40_repair.h"

errno_t bbox40_check_struct(reiser4_place_t *place, uint8_t mode) {
	uint64_t oid, type;
	uint64_t slink_oid;
	uint8_t size;
	
	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);

	if (type >= SL_LAST) {
		aal_error("Node (%llu), item (%u): safe link item (%s) of the "
			  "unknown type (%llu) found.", place->node->block->nr, 
			  place->pos.item, place->plug->label, type);
		
		return RE_FATAL;
	}
	
	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	if (type == SL_TRUNCATE)
		size += sizeof(uint64_t);

	if (size != place->len) {
		aal_error("Node (%llu), item (%u): safe link item (%s) of "
			  "the wrong length (%u) found. Should be (%u).", 
			  place->node->block->nr, place->pos.item, 
			  place->plug->label, place->len, size);
		
		return RE_FATAL;
	}

	oid = plug_call(place->key.plug->o.key_ops, 
			get_locality, &place->key);

	slink_oid = bbox40_core->tree_ops.slink_locality(place->node->tree);

	if (oid != slink_oid) {
		aal_error("Node (%llu), item (%u): safe link item (%s) with "
			  "the wrong locality (%llu) found. Should be (%llu).", 
			  place->node->block->nr, place->pos.item, 
			  place->plug->label, oid, slink_oid);
		
		return RE_FATAL;
	}

	return 0;
}

void bbox40_print(reiser4_place_t *place, aal_stream_t *stream, 
		  uint16_t options) 
{
	uint64_t type;
	uint8_t size;

	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);

	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	if (type == SL_TRUNCATE)
		size += sizeof(uint64_t);
	
	if (place->len != size) {
		aal_stream_format(stream, "Broken item.\n");
		return;
	}
	
	aal_stream_format(stream, "  %s  %s",
			  bbox40_core->key_ops.print(&place->key, PO_DEFAULT),
			  reiser4_slink_name[type]);
	
	if (type == SL_TRUNCATE) {
		uint64_t *len = (uint64_t *)(place->body + size - 
					     sizeof(uint64_t));
		
		aal_stream_format(stream, "%llu", *len);
	}
	
	aal_stream_format(stream, "\n");
}

#endif
