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
	ET40_PREPEND	= 1 << 1,
	/* Add some units at the end. */
	ET40_APPEND	= 1 << 2,
	/* Add some units at the end. */
	ET40_OVERWRITE	= 1 << 3,
	/* Join the first src unit with dst's one. */
	ET40_JOIN_START	= 1 << 4,
	/* Join the last src unit with dst's one. */
	ET40_JOIN_END	= 1 << 5,
	/* Remove the currend dst unit. */
	ET40_RM		= 1 << 6
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
		aal_exception_error("Node (%llu), item (%u), unit (%u): "
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
		aal_exception_error("Node (%llu), item (%u): extent40 "
				    "item of not valid length found.",
				    place->block->nr, place->pos.item);
		return RE_FATAL;
	}
	
	/* Offset must be divisible by block size. */
	if (plug_call(place->key.plug->o.key_ops, get_offset, &place->key) %
	    extent40_blksize(place)) 
	{
		aal_exception_error("Node (%llu), item (%u): extent40 item "
				    "with not valid key offset found.", 
				    place->block->nr, place->pos.item);
		return RE_FATAL;
	}
	
	extent = extent40_body(place);
	units = extent40_units(place);
	
	if (!units) {
		aal_exception_error("Node (%llu), item (%u): extent40 "
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

		aal_exception_error("Node (%llu), item (%u), unit (%u): "
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
	uint64_t offset;
	uint32_t send;
	place_t *src;
	
	aal_assert("vpf-1372", place != NULL);
	aal_assert("vpf-1373", hint != NULL);

	src = (place_t *)hint->specific;

	sextent = extent40_body(src);
	dextent = extent40_body(place);
	hint->flags = 0;
	
	offset = plug_call(hint->offset.plug->o.key_ops, get_offset, &place->key);

	if (place->pos.unit == MAX_UINT32) {
		/* The whole item to be inserted. */
		hint->flags |= ET40_ADD;
		send = extent40_units(src) - 1;
	} else if (place->pos.unit == extent40_units(place)) {
		/* The item tail to be inserted. */
		hint->flags |= ET40_APPEND;
		send = extent40_units(src) - 1;

		if (extent40_join(place, src, &hint->offset))
			hint->flags |= ET40_JOIN_START;
		
		/* Get the tail within the start @src unit. */
		hint->head = et40_get_width(sextent + src->pos.unit) - 
			extent40_head(src, src->pos.unit, &hint->offset);
	} else if (plug_call(place->key.plug->o.key_ops, compfull, 
			     &hint->offset, &place->key) < 0)
	{
		/* The item head to be inserted. */		
		hint->flags |= ET40_PREPEND;

		/* Get the @src end unit and the tail within it. */
		offset -= plug_call(hint->offset.plug->o.key_ops,
				    get_offset, &src->key);
		
		send = extent40_unit(src, offset - 1);
		hint->tail = extent40_head(src, send, &place->key);

		/* If the last block to be inserted is by the first 
		   existent, join them. */
		if (extent40_join(src, place, &place->key))
			hint->flags |= ET40_JOIN_END;
	} else if (!et40_get_start(dextent + place->pos.unit) &&
		   et40_get_start(sextent + src->pos.unit)) 
	{
		/* Estimate the overwrite. */
		
		key_entity_t key;
		uint64_t head;
		
		hint->flags |= ET40_OVERWRITE;
		head = extent40_head(place, place->pos.unit, &hint->offset);

		if (extent40_join(place, src, &hint->offset))
			hint->flags |= ET40_JOIN_START;
		
		hint->head = et40_get_width(sextent + send) - 
			extent40_head(src, src->pos.unit, &hint->offset);
		
		/* If can be joint with the previous dst unit, do just 
		   joining, overwriting will be performed later. */
		if ((hint->flags & ET40_JOIN_START) && !head) {
			head = et40_get_width(dextent + place->pos.unit);
			
			if (hint->head >= head) {
				/* Remove the current dst unit. */
				hint->flags |= ET40_RM;
				/* Handle only @head blocks. */
				hint->head = head;
			}

			/* Overwrite @hint->head blocks. */
			offset += hint->head * extent40_blksize(place);
		} else {
			/* Overwrite through the next dst unit key. */
			offset += extent40_offset(place, place->pos.unit + 1);
			hint->flags |= ET40_RM;
		}
		
		send = extent40_unit(src, offset - 1);
		plug_call(src->key.plug->o.key_ops, assign, &key, &place->key);
		plug_call(src->key.plug->o.key_ops, set_offset, &key, offset);
		hint->tail = extent40_head(src, send, &key);

		if (extent40_join(src, place, &key))
			hint->flags |= ET40_JOIN_END;
		
	} else {
		/* The skip is estimated at merge. */
		send = src->pos.unit - 1;
	}
	
	hint->bytes = 0;
	hint->count = send + 1 - src->pos.unit;
	hint->len = (hint->count - (hint->flags & ET40_JOIN_START)
		- (hint->flags & ET40_JOIN_END) - (hint->flags & ET40_RM)) 
		* sizeof(extent40_t);
	hint->overhead = 0;
	
	return 0;
}

int64_t extent40_merge(place_t *place, trans_hint_t *hint) {
	return 0;
}

#if 0
/* FIXME-VITALY: Do not forget to handle the case with unit's @start == 0. */
errno_t extent40_prep_merge(place_t *dst, place_t *src, 
			    merge_hint_t *hint)
{
	uint64_t dst_max, src_min, src_max, src_end;
	uint64_t src_start, dst_start, dst_min;
	extent40_t *dst_body, *src_body;
	uint32_t dst_pos, src_pos;
	key_entity_t key;
	lookup_t lookup;
	uint64_t b_size;
	uint32_t pos;
	errno_t res;
	
	aal_assert("vpf-990", hint != NULL);
	aal_assert("vpf-991", dst  != NULL);
	aal_assert("vpf-992", src  != NULL);

	b_size = extent40_blksize(src);
	
	dst_pos = dst->pos.unit;
	src_pos = src->pos.unit;
	
	dst_body = extent40_body(dst);
	src_body = extent40_body(src);
	
	/* Getting src_start, dst_start, src_max, dst_max, dst_min and src_min. */
	src_end = plug_call(hint->end.plug->o.key_ops, get_offset, 
			    &hint->end) + 1;
	
	src_start = plug_call(hint->start.plug->o.key_ops, get_offset, 
			      &hint->start);
	
	src_min = plug_call(src->key.plug->o.key_ops, get_offset, 
			    &src->key);
	
	if ((res = extent40_maxreal_key(src, &key)))
		return res;
	
	src_max = plug_call(key.plug->o.key_ops, get_offset, &key) + 1;
	
	/* Copy through src_end only. */
	if (src_max > src_end)
		src_max = src_end;
	
	hint->src_head = (src_start - extent40_offset(src, src_pos) - 
			  src_min) / b_size;

	hint->src_tail = (src_max - src_min  - 
			  extent40_offset(src, extent40_unit(src, src_max - 1))) 
			 / b_size;
	
	if ((res = extent40_maxreal_key(dst, &key)))
		return res;
	
	dst_max = plug_call(key.plug->o.key_ops, get_offset, &key) + 1;
	
	aal_assert("vpf-996", src_start % b_size == 0);
	aal_assert("vpf-998", src_max % b_size == 0);
	aal_assert("vpf-999", dst_max % b_size == 0);
	aal_assert("vpf-1009", src_start < src_max);
	
	if (dst_pos >= extent40_units(dst)) {
		aal_assert("vpf-1007", src_start == dst_max);
		
		hint->dst_count = 0;
		hint->src_count = extent40_unit(src, src_max - 1 - src_min) - 
			src_pos + 1;
		
		hint->len_delta = sizeof(extent40_t) * hint->src_count;
		
		hint->dst_head = hint->dst_tail = 0;
		
		plug_call(hint->end.plug->o.key_ops, set_offset, 
			    &hint->end, src_max);
		
		return 0;
	}
	
	dst_min = plug_call(dst->key.plug->o.key_ops, get_offset, &dst->key);
	dst_start = extent40_offset(dst, dst_pos) + dst_min;    
	
	aal_assert("vpf-997", dst_start % b_size == 0);
	aal_assert("vpf-1010", dst_start < dst_max);
	aal_assert("vpf-1000", src_start < dst_start + 
		   et40_get_width(dst_body + dst_pos) * b_size);
	
	if (src_start <= dst_start) {
		/* Dst head is not chopped. */
		hint->dst_head = 0;
		hint->head = 0;
	} else if (src_start <= dst_max) {
		/* Dst head is chopped. */
		hint->dst_head = (src_start - dst_start) / b_size;
		
		if (hint->dst_head == 0)
			hint->head = 0;
		else if (et40_get_start(src_body + src_pos) + hint->src_head == 
			 et40_get_start(dst_body + dst_pos) + hint->dst_head)
			hint->head = 0;
		else 
			hint->head = 1;
	} else {
		aal_assert("vpf-1008: Must be handled already.", 
			   dst_pos >= extent40_units(dst));
	}
	
	if (dst_max < src_max)
		src_max = dst_max;
	
	plug_call(key.plug->o.key_ops, set_offset, &key, src_max - 1);    
	plug_call(hint->end.plug->o.key_ops, set_offset, &hint->end, src_max);
	
	if ((lookup = extent40_lookup(dst, &key, FIND_EXACT)) < 0)
		return lookup;

	aal_assert("vpf-1001", lookup == PRESENT);

	pos = dst->pos.unit;
	
	hint->dst_count = pos - dst_pos + 1;
	hint->src_count = extent40_unit(src, src_max - 1 - src_min) - src_pos + 1;
	hint->dst_tail = (src_max - extent40_offset(dst, pos) - dst_min) / b_size;
	
	if (hint->dst_tail == et40_get_width(dst_body + pos))
		hint->tail = 0;
	else if(et40_get_start(src_body + src_pos + hint->src_count - 1) + 
		hint->src_tail == 
		et40_get_start(dst_body + pos) + hint->dst_tail)
		hint->tail = 0;
	else
		hint->tail = 1;
	
	hint->len_delta = (hint->src_count - hint->dst_count + 
			   hint->head + hint->tail) * sizeof(extent40_t);
	
	return 0;
}

errno_t extent40_merge(place_t *dst, place_t *src, merge_hint_t *hint) {
	extent40_t *dst_body, *src_body;
	uint32_t dst_units, src_units;
	uint64_t dst_head, dst_tail;
	uint32_t dst_pos, src_pos;
	int32_t  move;
    
	aal_assert("vpf-993", hint != NULL);
	aal_assert("vpf-994", dst  != NULL);
	aal_assert("vpf-995", src  != NULL);
	
	dst_body = extent40_body(dst);
	src_body = extent40_body(src);
	
	/* Amount of units to be added. */
	move = hint->len_delta / sizeof(extent40_t);

	dst_pos = dst->pos.unit;
	src_pos = src->pos.unit;
	
	dst_units = extent40_units(dst);
	src_units = extent40_units(src);
	
	aal_assert("vpf-1017", dst_pos + hint->dst_count <= dst_units);
	aal_assert("vpf-1017", src_pos + hint->src_count <= src_units);
	
	dst_body += dst_pos;
	src_body += src_pos;
	
	aal_assert("vpf-1018", et40_get_width(dst_body) > hint->dst_head);
	aal_assert("vpf-1019", et40_get_width(src_body) > hint->src_head);
	aal_assert("vpf-1020", et40_get_width(dst_body + hint->dst_count - 1) > 
		   hint->dst_tail);
	aal_assert("vpf-1021", et40_get_width(src_body + hint->src_count - 1) > 
		   hint->src_tail);
	
	/* Result width in the first dst unit. */
	dst_head = hint->dst_head + 
		(hint->head ? 0 : et40_get_width(src_body) - hint->src_head);
	
	et40_set_width(dst_body, dst_head);
	
	/* If the first dst unit is merged with the first src one. */
	if (!hint->head) {
		dst_body++;
		src_body++;
		dst_pos++;
		src_pos++;
		hint->src_count--;
		hint->dst_count--;
	}
	
	/* Result width in the last dst unit. */
	dst_tail = et40_get_width(dst_body + hint->dst_count - 1) - hint->dst_tail;
	
	if (!hint->tail)
		dst_tail += hint->src_tail;
	
	aal_memcpy(dst_body + move, dst_body, 
		   (dst_units - dst_pos) * sizeof(extent40_t));
	
	et40_set_width(dst_body + hint->dst_count - 1 + move, dst_tail);
	
	if (!hint->tail) {
		hint->src_count--;
		hint->dst_count--;
	}
	
	if (!hint->src_count)
		return 0;
	
	aal_memcpy(dst_body, src_body, hint->src_count * sizeof(extent40_t));
	
	return 0;
}
#endif

#endif
