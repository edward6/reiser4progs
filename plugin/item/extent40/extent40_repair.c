/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40_repare.c -- repair dafault extent plugin methods. */

#ifndef ENABLE_MINIMAL

#include "extent40.h"
#include <repair/plugin.h>

static int extent40_join_units(reiser4_place_t *place, int fix) {
	uint32_t i, count, joint;
	extent40_t *extent;

	extent = extent40_body(place);
	count = extent40_units(place);
	joint = 0;
	
	for (i = 0; i < count; i++, extent++) {
		bool_t join = 0;

		/* width == 0. */
		if (!et40_get_width(extent))
			goto shrink;

		if (i == 0) continue;
	
		join = (et40_get_start(extent - 1) == 0 &&
			et40_get_start(extent) == 0);
		join = (et40_get_start(extent - 1) + et40_get_width(extent - 1)
			== et40_get_start(extent)) || join;
		
		if (!join) continue;
		
	shrink:
		joint++;
	
		if (!fix) continue;
		
		if (i) {
			et40_set_width(extent - 1, et40_get_width(extent - 1)
				       + et40_get_width(extent));
		}
		
		extent40_shrink(place, i, 1);
		count--;
		extent--;
		i--;
	}

	return joint;
}

errno_t extent40_check_layout(reiser4_place_t *place, 
			      repair_hint_t *hint, 
			      region_func_t func, 
			      void *data)
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
		
		if (start == EXTENT_HOLE_UNIT || 
		    start == EXTENT_UNALLOC_UNIT || 
		    width == 0)
		{
			continue;
		}

		if ((res = func(start, width, data)) < 0)
			return res;
		
		if (!res) continue;
		
		/* Zero the problem region. */
		fsck_mess("Node (%llu), item (%u), unit (%u), [%s]: "
			  "points %s region [%llu..%llu].%s", 
			  (unsigned long long)place_blknr(place),
			  place->pos.item, i,
			  print_key(extent40_core, &place->key),
			  res == RE_FATAL? "out of the fs," : 
			  "to the already used blocks, ",
			  (unsigned long long)start,
			  (unsigned long long)(start + width - 1),
			  hint->mode != RM_CHECK ? 
			  " Zeroed." : "");

		if (hint->mode != RM_CHECK) {
			et40_set_start(extent, 0);
			place_mkdirty(place);
		} else
			result = RE_FIXABLE;
	}
	if (hint->mode != RM_CHECK) {
		if ((units = extent40_join_units(place, 1))) {
			fsck_mess("Node (%llu), item (%u): %u mergable units "
				  "were found in the extent40 unit.%s", 
				  (unsigned long long)place_blknr(place),
				  place->pos.item, units, 
				  hint->mode == RM_CHECK ? "" : " Merged.");
			
			hint->len += (units * sizeof(extent40_t));
		}
	}
	
	return result;
}

errno_t extent40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	extent40_t *extent;
	uint32_t i, units;
	errno_t res = 0;
	
	aal_assert("vpf-750", place != NULL);
	
	/* Length must be divisible by the extent40 unit length. */
	if (place->len % sizeof(extent40_t)) {
		fsck_mess("Node (%llu), item (%u), [%s]: extent40 "
			  "item of not valid length found.", 
			  (unsigned long long)place_blknr(place),
			  place->pos.item,
			  print_key(extent40_core, &place->key));
		return RE_FATAL;
	}
	
	/* Offset must be divisible by block size. */
	if (objcall(&place->key, get_offset) % place_blksize(place)) {
		fsck_mess("Node (%llu), item (%u), [%s]: extent40 "
			  "item with not valid key offset found.",
			  (unsigned long long)place_blknr(place),
			  place->pos.item,
			  print_key(extent40_core, &place->key));
		return RE_FATAL;
	}
	
	extent = extent40_body(place);
	units = extent40_units(place);
	
	if (!units) {
		fsck_mess("Node (%llu), item (%u): empty extent40 item "
			  "found.",
			  (unsigned long long)place_blknr(place),
			  place->pos.item);
		return RE_FATAL;
	}
	
	/* Zero all unallocated units if any found. */
	for (i = 0; i < units; i++, extent++) {
		uint64_t start;

		if (!(start = et40_get_start(extent)))
			continue;
		
		if (start != EXTENT_UNALLOC_UNIT)
			continue;

		fsck_mess("Node (%llu), item (%u), unit (%u), "
			  "[%s]: unallocated unit is found.%s", 
			  (unsigned long long)place_blknr(place),
			  place->pos.item, i,
			  print_key(extent40_core, &place->key),
			  hint->mode == RM_CHECK ? "" : "Zeroed.");
		
		if (hint->mode != RM_CHECK) {
			et40_set_start(extent, 0);
			place_mkdirty(place);
		} else 
			res |= RE_FIXABLE;
	}
	
	if (hint->mode != RM_CHECK) {
		if ((units = extent40_join_units(place, 1))) {
			fsck_mess("Node (%llu), item (%u): %u mergable units "
				  "were found in the extent40 unit.%s", 
				  (unsigned long long)place_blknr(place),
				  place->pos.item, units, 
				  hint->mode == RM_CHECK ? "" : " Merged.");
			
			hint->len += (units * sizeof(extent40_t));
		}
	}
	
	return res;
}

static uint32_t extent40_head(reiser4_place_t *place, 
			      uint32_t pos, 
			      reiser4_key_t *key) 
{
	uint64_t koffset, offset, doffset;

	koffset = objcall(key, get_offset);
	offset  = objcall(&place->key, get_offset);

	aal_assert("vpf-1381", koffset >= offset);
	
	offset = koffset - offset;
	doffset = extent40_offset(place, pos);
	
	aal_assert("vpf-1787", offset >= doffset);
	
	return (offset - doffset) / place_blksize(place);
}

errno_t extent40_prep_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	extent40_t *sextent, *dextent;
	int32_t send, sunits;
	reiser4_place_t *src;
	uint64_t offset;
	
	aal_assert("vpf-1372", place != NULL);
	aal_assert("vpf-1373", hint != NULL);
	aal_assert("vpf-1382", hint->specific != NULL);

	src = (reiser4_place_t *)hint->specific;

	sextent = extent40_body(src);
	dextent = extent40_body(place);
	
	/* Get the offset of the dst item key. */
	offset = objcall(&place->key, get_offset);
	
	/* Probably all units will be inserted. */
	send = src->pos.unit - 1;
	sunits = extent40_units(src);
	
	/* Get the head within the first @src unit. Head should be calculated 
	   here because we may try to insert not from the beginning of the @src
	   in any case. */
	hint->head = extent40_head(src, src->pos.unit, &hint->offset);
	hint->tail = 0;
	hint->insert_flags = 0;

	if (place->pos.unit == MAX_UINT32 ||
	    place->pos.unit == extent40_units(place))
	{
		/* The whole item to be inserted. */
		send = sunits - 1;
	} else if (objcall(&hint->offset, compfull, &place->key) < 0) {
		/* Get the @src end unit and the tail within it. */
		offset -= objcall(&src->key, get_offset);
		
		send = extent40_unit(src, offset - 1);
		
		hint->tail = et40_get_width(sextent + send) - 
			extent40_head(src, send, &place->key);
	} else if (!et40_get_start(dextent + place->pos.unit) && 
		   et40_get_start(sextent + src->pos.unit)) 
	{
		/* Estimate the overwrite. */
		hint->insert_flags |= ET40_OVERWRITE;

		/* Overwrite through the next dst unit key. */
		offset += extent40_offset(place, place->pos.unit + 1);
		
		/* Get the end position. */
		offset -= objcall(&src->key, get_offset);
		send = extent40_unit(src, offset - 1);
		
		if (send < sunits) {
			hint->tail = (extent40_offset(src, send + 1) - offset)
				/ place_blksize(src);
		} else {
			/* The src item finished earlier then dst item by key 
			   offset, then some part of the dst item will not be
			   overwritten, set TAIL falg here. */
			send = sunits - 1;
			hint->tail = 0;
			hint->insert_flags |= ET40_TAIL;
		}
		
		if (extent40_head(place, place->pos.unit, &hint->offset))
			hint->insert_flags |= ET40_HEAD;
	} 
	
	hint->overhead = 0;
	hint->bytes = 0;
	
	hint->count = send + 1 - src->pos.unit;
	hint->len = hint->count;
	
	if (hint->insert_flags & ET40_OVERWRITE) {
		/* If this is an overrite, the current dst unit can be used 
		   instead of adding another one (if there is no head in dst)
		   and one more unit should be inserted (if there is a tail). */
		hint->len += (hint->insert_flags & ET40_TAIL ? 1 : 0)
			- (hint->insert_flags & ET40_HEAD ? 0 : 1);
	}
	
	hint->len *= sizeof(extent40_t);
	
	return 0;
}

int64_t extent40_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t i, sstart, dstart, count;
	extent40_t *sextent, *dextent;
	uint64_t head, tail, offset;
	reiser4_place_t *src;
	errno_t res;
	
	aal_assert("vpf-1383", place != NULL);
	aal_assert("vpf-1384", hint != NULL);
	aal_assert("vpf-1385", hint->specific != NULL);

	src = (reiser4_place_t *)hint->specific;
	sextent = extent40_body(src);
	dextent = extent40_body(place);

	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;
	
	/* @head & @tail were calculated for the start unit, but 
	   if ET40_HEAD flag is set, data should be inserted to 
	   the next unit. */
	dstart = place->pos.unit + 
		(hint->insert_flags & ET40_HEAD ? 1 : 0);
	
	sstart = src->pos.unit == MAX_UINT32 ? 0 : src->pos.unit;

	/* Set the maxkey of the passed operation. */
	aal_memcpy(&hint->maxkey, &hint->offset, sizeof(hint->maxkey));
	
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
			offset = objcall(&place->key, get_offset);
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
			offset = objcall(&src->key, get_offset);
			offset += extent40_offset(src, sstart);
		}
		
		objcall(&hint->maxkey, set_offset, offset);
		return 0;
	}

	count = hint->count;

	if (hint->insert_flags & ET40_OVERWRITE) {
		count += hint->insert_flags & ET40_TAIL ? 1 : 0;
		count -= hint->insert_flags & ET40_HEAD ? 0 : 1;
	}

	/* Set the maxkey offset correctly. */
	offset = objcall(&src->key, get_offset);

	offset += extent40_offset(src, sstart + hint->count);
	offset -= hint->tail * place_blksize(src);
	objcall(&hint->maxkey, set_offset, offset);

	tail = head = 0;
	
	if (hint->insert_flags & ET40_TAIL) {
		/* Get the amount of blocks to be left in the head. */
		tail = extent40_head(place, place->pos.unit, &hint->maxkey);
	} 
	
	if (hint->insert_flags & ET40_HEAD) {
		/* Get the amount of blocks to be left in the tail. */
		head = extent40_head(place, place->pos.unit, &hint->offset);
	} 

	/* Expanding extent item at @place */
	extent40_expand(place, dstart, count);

	/* If some tail should be cut off the current dst unit, set the 
	   correct width there. */
	if (hint->insert_flags & ET40_TAIL) {
		uint64_t width;
		
		/* Set the correct width. Start is 0 because allocated 
		   units are not overwritten. */
		et40_set_start(dextent + place->pos.unit + count, 0);
		width = et40_get_width(dextent + place->pos.unit) - tail;
		et40_set_width(dextent + place->pos.unit + count, width);

		/* Fix the current dst unit after cutting. */
		et40_set_width(dextent + place->pos.unit, tail);
	}

	/* If some head should be left in the current dst unit, set the 
	   correct width there. */
	if (hint->insert_flags & ET40_HEAD) {
		/* Get the amount of blocks to be left. */

		/* Fix the current dst unit. */
		et40_set_width(dextent + place->pos.unit, head);
	}

	dextent += dstart;
	sextent += sstart;
		
	aal_memcpy(dextent, sextent, 
		   hint->count * sizeof(extent40_t));
	
	if (hint->head) {
		/* Cut head blocks of the first unit copied from src, 
		   if it was not the hole (==0). */
		if ((dstart = et40_get_start(dextent)))
			et40_set_start(dextent, dstart + hint->head);
		
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
		uint64_t dwidth;
		
		dextent -= (hint->count - 1);
		
		for (i = 0; i < hint->count; i++, dextent++) {
			dstart = et40_get_start(dextent);
			dwidth = et40_get_width(dextent);
			
			if (dstart == EXTENT_UNALLOC_UNIT || 
			    dstart == EXTENT_HOLE_UNIT)
			{
				continue;
			}
			
			if ((res = hint->region_func(dstart, dwidth,
						     hint->data)))
			{
				return res;
			}
		}
	}
	
	/* Update the item key. */
	if (objcall(&hint->offset, compfull, &place->key) < 0)
		aal_memcpy(&place->key, &hint->offset, sizeof(place->key));
	
	place_mkdirty(place);
	
	return 0;
}

/* Prints extent item into specified @stream */
void extent40_print(reiser4_place_t *place, aal_stream_t *stream,
		    uint16_t options)
{
	uint32_t i, count;
	extent40_t *extent;
    
	aal_assert("umka-1205", place != NULL);
	aal_assert("umka-1206", stream != NULL);

	extent = extent40_body(place);
	count = extent40_units(place);

	aal_stream_format(stream, "\nUNITS=%u [", count);
		
	for (i = 0; i < count; i++) {
		aal_stream_format(stream, "%llu(%llu)%s",
				  et40_get_start(extent + i),
				  et40_get_width(extent + i),
				  (i < count - 1 ? " " : ""));
	}
	
	aal_stream_format(stream, "]\n");
}

#endif
