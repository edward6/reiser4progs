/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40_repare.c -- repair dafault extent plugin methods. */

#ifndef ENABLE_STAND_ALONE
#include "extent40.h"
#include <repair/plugin.h>

extern uint32_t extent40_units(place_t *place);

extern errno_t extent40_maxreal_key(place_t *place,
				    key_entity_t *key);

extern uint64_t extent40_offset(place_t *place, uint64_t pos);
extern uint32_t extent40_unit(place_t *place, uint64_t offset);

extern lookup_t extent40_lookup(place_t *place, key_entity_t *key, 
				bias_t bias);

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
				    "pointed region [%llu..%llu] is invalid.%s",
				    place->block->nr, place->pos.item, i, start,
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
	
	if (place->len % sizeof(extent40_t)) 
		return RE_FATAL;
	
	extent = extent40_body(place);
	units = extent40_units(place);
	
	for (i = 0; i < units; i++, extent++) {
		uint64_t start;

		if (!(start = et40_get_start(extent)))
			continue;
		
		if (start != EXTENT_UNALLOC_UNIT)
			continue;

		aal_exception_error("Node (%llu), unit (%u): unallocated "
				    "unit is found.%s", place->block->nr,
				    i, mode == RM_CHECK ? "" : "Zeroed.");

		if (mode != RM_CHECK) {
			et40_set_start(extent, 0);
			place_mkdirty(place);
		} else 
			res |= RE_FIXABLE;
	}
	
	return res;
}

errno_t extent40_merge(place_t *dst, place_t *src, 
		       merge_hint_t *hint)
{
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

/* FIXME-VITALY: Do not forget to handle the case with unit's @start == 0. */
errno_t extent40_estimate_merge(place_t *dst, place_t *src, 
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

	/* FIXME-UMKA->VITALY: Is it realy needed here to convert values
	   twice? */
	hint->src_tail = (src_max - 
			  extent40_offset(src, extent40_unit(src, src_max - 1)) - 
			  src_min) / b_size;
	
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
#endif
