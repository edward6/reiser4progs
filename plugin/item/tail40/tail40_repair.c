/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.c -- reiser4 default tail plugin. */

#ifndef ENABLE_STAND_ALONE
#include "tail40.h"
#include <repair/plugin.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

errno_t tail40_prep_merge(place_t *place, trans_hint_t *hint) {
	uint64_t doffset, start;
	place_t *src;
	
	aal_assert("vpf-982", place != NULL);
	aal_assert("vpf-983", hint != NULL);
	aal_assert("vpf-984", hint->specific != NULL);
	
	src = (place_t *)hint->specific;
	
	doffset = plug_call(place->key.plug->o.key_ops, get_offset,
			    &place->key);
	start = plug_call(hint->offset.plug->o.key_ops, get_offset,
			  &hint->offset);
	
	if (place->pos.unit == tail40_units(place) || 
	    place->pos.unit == MAX_UINT32)
		/* New item or appending to the end. */
		hint->count = tail40_units(src) - src->pos.unit;
	else if (start < doffset)
		/* Prepending. */
		hint->count = doffset - start;
	else 
		hint->count = 0;

	hint->overhead = 0;
	hint->bytes = 0;
	hint->len = hint->count;

	return 0;
}

errno_t tail40_merge(place_t *place, trans_hint_t *hint) {
	uint64_t offset;
	place_t *src;
	
	aal_assert("vpf-987", place != NULL);
	aal_assert("vpf-988", hint != NULL);

	src = (place_t *)hint->specific;
	
	offset = plug_call(hint->offset.plug->o.key_ops,
			   get_offset, &hint->offset);
	
	if (hint->count) {
		tail40_expand(place, place->pos.unit, hint->len);

		tail40_copy(place, place->pos.unit, src, 
			    src->pos.unit, hint->count);

		offset += hint->count;
	} else
		offset += tail40_units(place) - place->pos.unit;
	
	/* Set the maxkey of the passed operation. */
	plug_call(src->key.plug->o.key_ops, assign, 
		  &hint->maxkey, &hint->offset);
	plug_call(hint->maxkey.plug->o.key_ops, 
		  set_offset, &hint->maxkey, offset);

	return 0;
}

#endif
