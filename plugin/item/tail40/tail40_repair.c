/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.c -- reiser4 default tail plugin. */

#ifndef ENABLE_MINIMAL
#include "tail40.h"
#include <repair/plugin.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

errno_t tail40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	aal_assert("vpf-1508", place != NULL);
	
	if (!place->len) {
		fsck_mess("Node (%llu), item (%u): tail40 item of zero length "
			  "found.", place_blknr(place), place->pos.item);
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
	
	if (place->pos.unit == tail40_units(place) || 
	    place->pos.unit == MAX_UINT32)
	{
		/* New item or appending to the end. */
		hint->count = tail40_units(src) - src->pos.unit;
	} else {
		uint64_t doffset, start;
		
		doffset = plug_call(place->key.plug->o.key_ops, 
				    get_offset, &place->key);
		
		start = plug_call(hint->offset.plug->o.key_ops, 
				  get_offset, &hint->offset);

		if (start < doffset)
			/* Prepending. */
			hint->count = doffset - start;
		else 
			hint->count = 0;
	}

	hint->overhead = 0;
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
	
	offset = plug_call(hint->offset.plug->o.key_ops,
			   get_offset, &hint->offset);
	
	if (hint->count) {
		/* Expand @place & copy @hint->count units there from @src. */
		pos = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;

		if (place->pos.unit != MAX_UINT32)
			tail40_expand(place, place->pos.unit, hint->len);

		res = tail40_copy(place, pos, src, src->pos.unit, hint->count);
		if (res) return res;

		place_mkdirty(place);
		
		offset += hint->count;
	} else
		offset += tail40_units(place) - place->pos.unit;
	
	/* Set the maxkey of the passed operation. */
	plug_call(src->key.plug->o.key_ops, assign, 
		  &hint->maxkey, &hint->offset);

	plug_call(hint->maxkey.plug->o.key_ops, 
		  set_offset, &hint->maxkey, offset);

	/* Update the item key. */
	if (place->pos.unit == 0 && hint->count) {
		plug_call(place->key.plug->o.key_ops, assign,
			  &place->key, &hint->offset);
	}
	
	return 0;
}

errno_t tail40_pack(reiser4_place_t *place, aal_stream_t *stream) {
	return 0;
}

errno_t tail40_unpack(reiser4_place_t *place, aal_stream_t *stream) {
	return 0;
}

#endif
