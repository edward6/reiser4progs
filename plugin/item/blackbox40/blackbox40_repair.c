/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.c -- reiser4 default tail plugin. */


#ifndef ENABLE_STAND_ALONE
#include <repair/plugin.h>
#include "blackbox40_repair.h"

errno_t blackbox40_check_struct(reiser4_place_t *place, uint8_t mode) {
	uint64_t oid, type;
	uint64_t safe_oid;
	uint8_t size;
	
	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);

	if (type >= SAFE_LAST) {
		aal_error("Node (%llu), item (%u): safe link item (%s) of the "
			  "unknown type (%llu) found.", place->node->block->nr, 
			  place->pos.item, place->plug->label, type);
		
		return RE_FATAL;
	}
	
	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	if (type == SAFE_TRUNCATE)
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

	safe_oid = blackbox40_core->tree_ops.safe_locality(place->node->tree);

	if (oid != safe_oid) {
		aal_error("Node (%llu), item (%u): safe link item (%s) with "
			  "the wrong locality (%llu) found. Should be (%llu).", 
			  place->node->block->nr, place->pos.item, 
			  place->plug->label, oid, safe_oid);
		
		return RE_FATAL;
	}

	return 0;
}

#endif
