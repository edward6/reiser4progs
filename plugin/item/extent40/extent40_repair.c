/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40_repare.c -- repair dafault extent plugin methods. */

#ifndef ENABLE_STAND_ALONE

#include "extent40.h"
#include <repair/plugin.h>

typedef enum merge_flag {
	/* Add an item. */
	ET40_ADD	= 1 << 0,
	/* Add some units at the beginning. */
	ET40_INSERT	= 1 << 1,
	/* Add some units at the end. */
	ET40_OVERWRITE	= 1 << 2,
	/* There is no head left in the current dst unit while overwriting. */
	ET40_HEAD	= 1 << 3,
	/* There is a tail left in the current dst unit while overwriting. */
	ET40_TAIL	= 1 << 4
} merge_flag_t;

errno_t extent40_check_layout(place_t *place, region_func_t func, 
			      void *data, uint8_t mode) 
{
	extent40_t *extent;
	uint32_t i, units;
	errno_t result = 0;
	
	aal_assert("vpf-724", place != NULL);
	aal_assert("vpf-725", func != NULL);

	extent = extent40_body(place);
	units = extent40_units(place);
			
	for (i = 0; i < units; i++, extent++) {
		uint64_t start, width;
		errno_t res;

		start = et40_get_start(extent);
		width = et40_get_width(extent);
		
		if (!start || start == EXTENT_UNALLOC_UNIT)
			continue;

		if ((res = func(place, start, width, data)) < 0)
			return res;
		
		if (!res) continue;
		
		/* Zero the problem region. */
		aal_error("Node (%llu), item (%u), unit (%u): "
			  "points %s region [%llu..%llu].%s",
			  place->block->nr, place->pos.item, i, 
			  res == RE_FATAL? "out of the fs," : 
			  "to the already used blocks, ", start,
			  start + width - 1, mode != RM_CHECK ? 
			  " Zeroed." : "");

		if (mode != RM_CHECK) {
			et40_set_start(extent, 0);
			place_mkdirty(place);
		} else
			result = RE_FIXABLE;
	}
	
	return result;
}

errno_t extent40_check_struct(place_t *place, uint8_t mode) {
	extent40_t *extent;
	uint32_t i, units;
	errno_t res = 0;
	
	aal_assert("vpf-750", place != NULL);
	
	/* Length must be divisible by the extent40 unit length. */
	if (place->len % sizeof(extent40_t)) {
		aal_error("Node (%llu), item (%u): extent40 "
			  "item of not valid length found.",
			  place->block->nr, place->pos.item);
		return RE_FATAL;
	}
	
	/* Offset must be divisible by block size. */
	if (plug_call(place->key.plug->o.key_ops, get_offset, &place->key) %
	    extent40_blksize(place)) 
	{
		aal_error("Node (%llu), item (%u): extent40 item "
			  "with not valid key offset found.", 
			  place->block->nr, place->pos.item);
		return RE_FATAL;
	}
	
	extent = extent40_body(place);
	units = extent40_units(place);
	
	if (!units) {
		aal_error("Node (%llu), item (%u): extent40 "
			  "item with no units found.",
			  place->block->nr, place->pos.item);
		return RE_FATAL;
	}
	
	/* Zero all unallocated units if any found. */
	for (i = 0; i < units; i++, extent++) {
		uint64_t start;

		if (!(start = et40_get_start(extent)))
			continue;
		
		if (start != EXTENT_UNALLOC_UNIT)
			continue;

		aal_error("Node (%llu), item (%u), unit (%u): "
			  "unallocated unit is found.%s",
			  place->block->nr, place->pos.item, i, 
			  mode == RM_CHECK ? "" : "Zeroed.");
		
		if (mode != RM_CHECK) {
			et40_set_start(extent, 0);
			place_mkdirty(place);
		} else 
			res |= RE_FIXABLE;
	}
	
	return res;
}

static inline int extent40_join(place_t *dst, place_t *src, key_entity_t *key) {
	uint64_t koffset, doffset, soffset;
	uint32_t dpos, spos, dtail, stail;
	extent40_t *dextent, *sextent;
	
	dextent = extent40_body(dst);
	sextent = extent40_body(src);

	koffset = plug_call(key->plug->o.key_ops, get_offset, key);
	doffset = plug_call(dst->key.plug->o.key_ops, get_offset, &dst->key);
	soffset = plug_call(src->key.plug->o.key_ops, get_offset, &src->key);
	
	if (koffset <= doffset || koffset <= soffset)
		return 0;

	doffset = koffset - doffset;
	soffset = koffset - soffset;

	dpos = extent40_unit(dst, doffset - 1);
	spos = extent40_unit(src, soffset);

	aal_assert("vpf-1379", dpos < extent40_units(dst));
	aal_assert("vpf-1380", spos < extent40_units(src));

	/* If both units are unallocated -- join them. */
	if (!et40_get_start(dextent + dpos) && !et40_get_start(sextent + spos))
		return 1;
	
	/* If just 1 unit is unallocated -- cannot join. */
	if (!et40_get_start(dextent + dpos) || !et40_get_start(sextent + spos))
		return 0;
	
	dtail = (doffset - extent40_offset(dst, dpos)) / extent40_blksize(dst);
	stail = (soffset - extent40_offset(src, spos)) / extent40_blksize(src);

	return et40_get_start(dextent + dpos) + dtail ==
	       et40_get_start(sextent + spos) + stail;
}

static inline uint32_t extent40_head(place_t *place, 
				     uint32_t pos, 
				     key_entity_t *key) 
{
	uint64_t koffset, offset, doffset;

	koffset = plug_call(key->plug->o.key_ops, get_offset, key);
	offset = plug_call(key->plug->o.key_ops, get_offset, &place->key);

	aal_assert("vpf-1381", koffset >= offset);
	
	offset = koffset - offset;
	doffset = extent40_offset(place, pos);
	
	aal_assert("vpf-1381", offset >= doffset);
	
	return (offset - doffset) / extent40_blksize(place);
}

errno_t extent40_prep_merge(place_t *place, trans_hint_t *hint) {
	extent40_t *sextent, *dextent;
	int32_t send, sunits;
	uint64_t offset;
	place_t *src;
	
	aal_assert("vpf-1372", place != NULL);
	aal_assert("vpf-1373", hint != NULL);
	aal_assert("vpf-1382", hint->specific != NULL);

	src = (place_t *)hint->specific;

	sextent = extent40_body(src) + src->pos.unit;
	dextent = extent40_body(place) + place->pos.unit;
	
	/* Get the offset of the dst item key. */
	offset = plug_call(hint->offset.plug->o.key_ops, 
			   get_offset, &place->key);
	
	/* Probably all units will be inserted. */
	send = src->pos.unit - 1;
	sunits = extent40_units(src);
	
	/* Get the head within the first @src unit. */
	hint->head = extent40_head(src, src->pos.unit, &hint->offset);
	hint->tail = 0;
	hint->flags = 0;

	if (place->pos.unit == extent40_units(place) || 
	    place->pos.unit == MAX_UINT32)
	{
		/* The whole item to be inserted. */
		send = extent40_units(src) - 1;
	} else if (plug_call(place->key.plug->o.key_ops, compfull, 
			     &hint->offset, &place->key) < 0)
	{
		/* The item head to be inserted. */		
		/* Get the @src end unit and the tail within it. */
		offset -= plug_call(hint->offset.plug->o.key_ops,
				    get_offset, &src->key);
		
		send = extent40_unit(src, offset - 1);
		hint->tail = et40_get_width(sextent + send) - 
			extent40_head(src, send, &place->key);
	} else if (!et40_get_start(dextent) && et40_get_start(sextent)) {
		/* Estimate the overwrite. */
		hint->flags |= ET40_OVERWRITE;

		/* Overwrite through the next dst unit key. */
		offset += extent40_offset(place, place->pos.unit + 1);
		
		/* Get the end position. */
		offset -= plug_call(hint->offset.plug->o.key_ops,
				    get_offset, &src->key);
		send = extent40_unit(src, offset - 1);
		
		if (send < sunits) {
			hint->tail = (extent40_offset(src, send + 1) - offset)
				/ extent40_blksize(src);
		} else {
			send = sunits - 1;
			hint->tail = 0;
			hint->flags |= ET40_TAIL;
		}
		
		if (extent40_head(place, place->pos.unit, &hint->offset))
			hint->flags |= ET40_HEAD;
	} 
	
	hint->overhead = 0;
	hint->bytes = 0;
	
	hint->count = send + 1 - src->pos.unit;
	hint->len = hint->count;
	
	if (hint->flags & ET40_OVERWRITE) {
		hint->len += (hint->flags & ET40_TAIL ? 1 : 0)
			- (hint->flags & ET40_HEAD ? 0 : 1);
	}
	
	hint->len *= sizeof(extent40_t);
	
	return 0;
}

int64_t extent40_merge(place_t *place, trans_hint_t *hint) {
	uint32_t i, sstart, dstart, count;
	extent40_t *sextent, *dextent;
	uint64_t head, tail, offset;
	place_t *src;
	errno_t res;
	
	aal_assert("vpf-1383", place != NULL);
	aal_assert("vpf-1384", hint != NULL);
	aal_assert("vpf-1385", hint->specific != NULL);

	src = (place_t *)hint->specific;
	sextent = extent40_body(src);
	dextent = extent40_body(place);

	dstart = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;
	sstart = src->pos.unit == MAX_UINT32 ? 0 : src->pos.unit;
	dstart += (hint->flags & ET40_HEAD ? 1 : 0);
	
	/* Set the maxkey of the passed operation. */
	plug_call(src->key.plug->o.key_ops, assign, &hint->maxkey, 
		  &hint->offset);
	
	if (!hint->count) {
		/* If there is nothing to be done, skip as much as possible. */
		if (et40_get_start(dextent + dstart)) {
			/* Skip all not zero pointers in @dst. */
			count = extent40_units(place);
		
			while (dstart < count && 
			       et40_get_start(dextent + dstart)) 
			{
				dstart++;
			}

			/* Get the offset for the maxkey. */
			offset = plug_call(hint->offset.plug->o.key_ops,
					   get_offset, &place->key);
		offset += extent40_offset(place, dstart);
		} else {
			uint32_t scount;
			
			/* Skip all zero pointers in @src. */
			scount = extent40_units(src);
			
			while (sstart < scount && 
			       !et40_get_start(sextent + sstart)) 
			{
				sstart++;
			}
			
			/* Get the offset for the maxkey. */
			offset = plug_call(hint->offset.plug->o.key_ops,
					   get_offset, &src->key);
			offset += extent40_offset(src, sstart);
		}
		
		plug_call(hint->offset.plug->o.key_ops, set_offset, 
			  &hint->maxkey, offset);

		return 0;
	}
	
	/* Get the start key offset. */
	offset = plug_call(hint->offset.plug->o.key_ops,
			   get_offset, &hint->offset);

	count = hint->count + (hint->flags & ET40_TAIL ? 1 : 0)
		- (hint->flags & ET40_HEAD ? 0 : 1);

	/* Set the maxkey offset correctly. */
	offset += extent40_offset(src, src->pos.unit + hint->count);
	offset -= hint->tail * extent40_blksize(src);
	plug_call(hint->offset.plug->o.key_ops, set_offset, 
		  &hint->maxkey, offset);

	if (hint->flags & ET40_TAIL) {
		/* Get the amount of blocks to be left in the head. */
		tail = extent40_head(place, place->pos.unit, &hint->maxkey);
	}
	
	if (hint->flags & ET40_HEAD) {
		/* Get the amount of blocks to be left in the tail. */
		head = extent40_head(place, place->pos.unit, &hint->offset);
	}
	
	/* Expanding extent item at @place */
	extent40_expand(place, dstart, count);

	/* If some tail should be cut off the current dst unit, set the 
	   correct width there. */
	if (hint->flags & ET40_TAIL) {
		/* Set the correct width. */
		et40_set_start(dextent + dstart + count - 1, 0);
		et40_set_width(dextent + dstart + count - 1, 
			       et40_get_width(dextent + place->pos.unit) 
			       - tail);

		/* Fix the current dst unit after cutting. */
		et40_set_width(dextent + place->pos.unit, tail);
	}

	/* If some head should be left in the current dst unit, set the 
	   correct width there. */
	if (hint->flags & ET40_HEAD) {
		/* Get the amount of blocks to be left. */

		/* Fix the current dst unit. */
		et40_set_width(dextent + place->pos.unit, head);

		/* Move to the next unit. */
		dextent++;
	}

	dextent += dstart;
	sextent += sstart;
		
	aal_memcpy(dextent, sextent, 
		   hint->count * sizeof(extent40_t));
	
	if (hint->head) {
		et40_set_start(dextent, et40_get_start(dextent) + 
			       hint->head);
		et40_set_width(dextent, et40_get_width(dextent) - 
			       hint->head);
	}
	
	/* Fix the tail unit. */
	dextent += hint->count - 1;
	if (hint->tail) {
		et40_set_width(dextent, et40_get_width(dextent) -
			       hint->tail);
	}
	
	/* Call region_func for all inserted regions. */
	if (hint->region_func) {
		dextent -= (hint->count - 1);
		
		for (i = 0; i < hint->count; i++, dextent++) {
			res = hint->region_func(NULL, et40_get_start(dextent),
						et40_get_width(dextent), 
						hint->data);
			
			if (res) return res;
		}
	}
	
	/* Join mergable units within the @place. */
	dextent = extent40_body(place);
	count = extent40_units(place);
	hint->len = 0;
	
	for (dstart = 1, dextent++; dstart < count; dstart++, dextent++) {
		if (et40_get_start(dextent - 1) + et40_get_width(dextent - 1)
		    == et40_get_start(dextent))
		{
			et40_set_width(dextent - 1, et40_get_width(dextent - 1)
				       + et40_get_width(dextent));

			extent40_shrink(place, dstart, 1);
			count--;
			hint->len--;
		}
	}
	
	/* Shrink the item when get back to caller. */
	hint->len *= sizeof(extent40_t);
	
	place_mkdirty(place);
	
	return 0;
}

/* Prints extent item into specified @stream */
void extent40_print(place_t *place, aal_stream_t *stream, uint16_t options) {
	uint32_t i, count;
	extent40_t *extent;
    
	aal_assert("umka-1205", place != NULL);
	aal_assert("umka-1206", stream != NULL);

	extent = extent40_body(place);
	count = extent40_units(place);

	aal_stream_format(stream, "UNITS=%u\n[", count);
		
	for (i = 0; i < count; i++) {
		aal_stream_format(stream, "%llu(%llu)%s",
				  et40_get_start(extent + i),
				  et40_get_width(extent + i),
				  (i < count - 1 ? " " : ""));
	}
	
	aal_stream_format(stream, "]\n");
}
#endif
