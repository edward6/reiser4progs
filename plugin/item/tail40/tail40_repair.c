/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40_repair.c -- reiser4 default tail plugin. */

#ifndef ENABLE_STAND_ALONE
#include "tail40.h"
#include <repair/plugin.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

errno_t tail40_merge(place_t *place, trans_hint_t *hint) {
/*
	uint32_t dst_pos, src_pos;
	
	aal_assert("vpf-987", dst  != NULL);
	aal_assert("vpf-988", src  != NULL);
	aal_assert("vpf-989", hint != NULL);

	dst_pos = dst->pos.unit;
	src_pos = src->pos.unit;
	
	aal_memcpy(tail40_body(dst) + dst_pos + hint->len_delta, 
		   tail40_body(dst) + dst_pos,
		   tail40_units(dst) - dst_pos);

	return tail40_copy(dst, dst_pos, src, src_pos, hint->src_count);
*/
	return 0;
}

errno_t tail40_prep_merge(place_t *place, trans_hint_t *hint) {
/*
	uint64_t src_offset, dst_max;
	uint64_t src_max, src_end;
	uint32_t dst_pos, src_pos;
	key_entity_t key;
	errno_t res;
	
	aal_assert("vpf-982", hint != NULL);
	aal_assert("vpf-983", dst  != NULL);
	aal_assert("vpf-984", src  != NULL);
	
	dst_pos = dst->pos.unit;
	src_pos = src->pos.unit;
	
	/ Getting src_offset, dst_offset, src_max and dst_max offsets.
	if ((res = tail40_maxreal_key(dst, &key)))
		return res;
	
	dst_max = plug_call(key.plug->o.key_ops, get_offset, &key);
	
	src_end = plug_call(hint->end.plug->o.key_ops, get_offset, &hint->end);
	
	if ((res = tail40_maxreal_key(src, &key)))
		return res;
	
	src_max = plug_call(key.plug->o.key_ops, get_offset, &key);
	
	if (src_max > src_end)
		src_max = src_end;
	
	src_offset = plug_call(hint->start.plug->o.key_ops, get_offset, 
				 &hint->start);
	
	if (src_offset <= dst_max) {
		uint64_t dst_offset;
		
		aal_assert("vpf-1005", dst_pos < tail40_units(dst));
		
		if (body40_get_key(dst, dst_pos, &key, NULL))
			return -EINVAL;
		
		dst_offset = plug_call(key.plug->o.key_ops, get_offset, &key);
		
		aal_assert("vpf-985", dst_offset < src_max);
		
		hint->src_count = (dst_max < src_max ? dst_max : src_max) - 
			src_offset + 1;
		
		hint->len_delta = dst_offset - src_offset;
	} else {
		aal_assert("vpf-986", src_offset == dst_max + 1);
		
		hint->src_count = hint->len_delta = src_max - src_offset + 1;
	}
	
	plug_call(hint->end.plug->o.key_ops, set_offset, &hint->end, 
		    src_offset + hint->src_count);
*/
	return 0;
}
#endif
