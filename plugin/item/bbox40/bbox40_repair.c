/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bbox40_repair.c -- reiser4 default black box plugin. */


#ifndef ENABLE_MINIMAL
#include <repair/plugin.h>
#include "bbox40_repair.h"

errno_t bbox40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	uint64_t type;
	uint8_t size;
	
	/* FIXME: this is hardcoded, type should be obtained in another way. */
	type = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);

	if (type >= SL_LAST) {
		fsck_mess("Node (%llu), item (%u), [%s]: safe link "
			  "item (%s) of the unknown type (%llu) found.",
			  place_blknr(place), place->pos.item, 
			  print_key(bbox40_core, &place->key),
			  place->plug->label, type);
		
		return RE_FATAL;
	}
	
	size = plug_call(place->key.plug->o.key_ops, bodysize) * 
		sizeof(uint64_t);

	if (type == SL_TRUNCATE)
		size += sizeof(uint64_t);

	if (size != place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: safe link item (%s) "
			  "of the wrong length (%u) found. Should be (%u).",
			  place_blknr(place), place->pos.item, 
			  print_key(bbox40_core, &place->key),
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

	aal_stream_format(stream, "    %s  %s",
			  print_key(bbox40_core, &key),
			  reiser4_slink_name[type]);
	
	if (type == SL_TRUNCATE) {
		uint64_t *len = (uint64_t *)(place->body + size);
		aal_stream_format(stream, "%llu", *len);
	}
	
	aal_stream_format(stream, "\n");
}

errno_t bbox40_prep_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_place_t *src;
	
	aal_assert("vpf-1662", place != NULL);
	aal_assert("vpf-1663", hint != NULL);
	aal_assert("vpf-1664", hint->specific != NULL);
	
	src = (reiser4_place_t *)hint->specific;

	hint->overhead = 0;
	hint->bytes = 0;

	if (place->pos.unit == MAX_UINT32) {
		hint->count = 1;
		hint->len = src->len;
	} else {
		hint->count = hint->len = 0;
	}
	
	return 0;
}

errno_t bbox40_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_place_t *src;
	
	aal_assert("vpf-1665", place != NULL);
	aal_assert("vpf-1666", hint != NULL);
	aal_assert("vpf-1667", hint->specific != NULL);

	if (!hint->len) return 0;
	
	src = (reiser4_place_t *)hint->specific;
	aal_memcpy(place->body, src->body, hint->len);
	place_mkdirty(place);
	
	return 0;
}

#endif
