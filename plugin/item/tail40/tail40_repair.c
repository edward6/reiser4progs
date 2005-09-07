/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.c -- reiser4 default tail plugin. */

#ifndef ENABLE_MINIMAL
#include "tail40.h"
#include <repair/plugin.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

errno_t tail40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	aal_assert("vpf-1508", place != NULL);
	
	if (place->len <= place->off) {
		fsck_mess("Node (%llu), item (%u): %s item of zero length "
			  "found.", place_blknr(place), place->pos.item, 
			  place->plug->label);
		return RE_FATAL;
	} 
	
	return 0;
}

errno_t tail40_prep_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_place_t *src;
	
	aal_assert("vpf-982", place != NULL);
	aal_assert("vpf-983", hint != NULL);
	aal_assert("vpf-984", hint->specific != NULL);
	
	src = (reiser4_place_t *)hint->specific;
	
	if (tail40_pos(place) == tail40_units(place) || 
	    place->pos.unit == MAX_UINT32)
	{
		/* New item or appending to the end. */
		hint->count = tail40_units(src) - tail40_pos(src);
	} else {
		uint64_t doffset, start;
		
		doffset = plug_call(place->key.plug->pl.key, 
				    get_offset, &place->key);
		
		start = plug_call(hint->offset.plug->pl.key, 
				  get_offset, &hint->offset);

		if (start < doffset)
			/* Prepending. */
			hint->count = doffset - start;
		else 
			hint->count = 0;
	}

	hint->overhead = (place->pos.unit == MAX_UINT32) ? place->off : 0;
	hint->bytes = 0;
	hint->len = hint->count;

	return 0;
}

errno_t tail40_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_place_t *src;
	uint64_t offset;
	uint32_t pos;
	errno_t res;
	
	aal_assert("vpf-987", place != NULL);
	aal_assert("vpf-988", hint != NULL);

	src = (reiser4_place_t *)hint->specific;
	pos = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;

	offset = plug_call(hint->offset.plug->pl.key,
			   get_offset, &hint->offset);
	
	if (hint->count) {
		/* Expand @place & copy @hint->count units there from @src. */
		if (place->pos.unit != MAX_UINT32)
			tail40_expand(place, place->pos.unit, hint->len);

		res = tail40_copy(place, pos, src, src->pos.unit, hint->count);
		if (res) return res;

		place_mkdirty(place);
		
		offset += hint->count;
	} else
		offset += tail40_units(place) - pos;
	
	/* Set the maxkey of the passed operation. */
	aal_memcpy(&hint->maxkey, &hint->offset, sizeof(hint->maxkey));

	plug_call(hint->maxkey.plug->pl.key, 
		  set_offset, &hint->maxkey, offset);

	/* Update the item key. */
	if (pos == 0 && hint->count) {
		aal_memcpy(&place->key, &hint->offset, sizeof(place->key));
	}
	
	return 0;
}

errno_t tail40_pack(reiser4_place_t *place, aal_stream_t *stream) {
	aal_assert("vpf-1767", place != NULL);
	aal_assert("vpf-1768", stream != NULL);
	
	if (place->off) {
		aal_stream_write(stream, place->body, place->off);
	}
	
	return 0;
}

errno_t tail40_unpack(reiser4_place_t *place, aal_stream_t *stream) {
	aal_assert("vpf-1769", place != NULL);
	aal_assert("vpf-1770", stream != NULL);
	
	if (place->off) {
		aal_stream_read(stream, place->body, place->off);
	}

	return 0;
}

#endif
