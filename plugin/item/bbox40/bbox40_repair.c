/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bbox40_repair.c -- reiser4 default black box plugin. */


#ifndef ENABLE_STAND_ALONE
#include <repair/plugin.h>
#include "bbox40_repair.h"

errno_t bbox40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	uint64_t type;
	uint8_t size;
	
	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);

	if (type >= SL_LAST) {
		fsck_mess("Node (%llu), item (%u): safe link item (%s) of the "
			  "unknown type (%llu) found.", place_blknr(place), 
			  place->pos.item, place->plug->label, type);
		
		return RE_FATAL;
	}
	
	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	if (type == SL_TRUNCATE)
		size += sizeof(uint64_t);

	if (size != place->len) {
		fsck_mess("Node (%llu), item (%u): safe link item (%s) of "
			  "the wrong length (%u) found. Should be (%u).", 
			  place_blknr(place), place->pos.item, 
			  place->plug->label, place->len, size);
		
		return RE_FATAL;
	}

	return 0;
}

void bbox40_print(reiser4_place_t *place, aal_stream_t *stream, 
		  uint16_t options) 
{
	reiser4_key_t key;
	uint64_t type;
	uint16_t size, trunc;

	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);

	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	trunc = (type == SL_TRUNCATE) ? sizeof(uint64_t) : 0;
	
	if (place->len != (uint32_t)size + trunc) {
		aal_stream_format(stream, "Broken item.\n");
		return;
	}
	
	aal_memcpy(&key, &place->key, sizeof(key));
	aal_memcpy(&key.body, place->body, size);

	aal_stream_format(stream, "UNITS=1\n    %s  %s",
			  bbox40_core->key_ops.print(&key, PO_DEFAULT),
			  reiser4_slink_name[type]);
	
	if (type == SL_TRUNCATE) {
		uint64_t *len = (uint64_t *)(place->body + size);
		aal_stream_format(stream, "%llu", *len);
	}
	
	aal_stream_format(stream, "\n");
}

#endif
