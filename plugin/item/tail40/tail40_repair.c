/*
    tail40_repair.c -- reiser4 default tail plugin.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include <reiser4/plugin.h>
#include <repair/repair_plugin.h>

#define tail40_body(item) (item->body)

extern errno_t tail40_get_key(item_entity_t *item, uint32_t pos, 
    key_entity_t *key);

extern errno_t tail40_maxreal_key(item_entity_t *item, key_entity_t *key);

extern errno_t tail40_rep(item_entity_t *dst_item, uint32_t dst_pos,
			  item_entity_t *src_item, uint32_t src_pos,
			  uint32_t count);

extern uint32_t tail40_units(item_entity_t *item);

errno_t tail40_copy(item_entity_t *dst, uint32_t dst_pos, 
    item_entity_t *src, uint32_t src_pos, copy_hint_t *hint)
{
    aal_assert("vpf-987", dst  != NULL);
    aal_assert("vpf-988", src  != NULL);
    aal_assert("vpf-989", hint != NULL);
    
    aal_memcpy(tail40_body(dst) + dst_pos + hint->len_delta, 
	       tail40_body(dst) + dst_pos, tail40_units(dst) - dst_pos);
    
    return tail40_rep(dst, dst_pos, src, src_pos, hint->src_count);
}

errno_t tail40_estimate_copy(item_entity_t *dst, uint32_t dst_pos, 
    item_entity_t *src, uint32_t src_pos, copy_hint_t *hint)
{
    uint64_t src_offset, dst_max, src_max, src_end;
    key_entity_t key;
    errno_t res;
    
    aal_assert("vpf-982", hint != NULL);
    aal_assert("vpf-983", dst  != NULL);
    aal_assert("vpf-984", src  != NULL);
    
    /* Getting src_offset, dst_offset, src_max and dst_max offsets. */
    if ((res = tail40_maxreal_key(dst, &key)))
	return res;

    dst_max = plugin_call(key.plugin->o.key_ops, get_offset, &key);
    
    src_end = plugin_call(hint->end.plugin->o.key_ops, get_offset, &hint->end);
    
    if ((res = tail40_maxreal_key(src, &key)))
	return res;
    
    src_max = plugin_call(key.plugin->o.key_ops, get_offset, &key);
    
    if (src_max > src_end)
	src_max = src_end;
	
    src_offset = plugin_call(hint->start.plugin->o.key_ops, get_offset, 
	&hint->start);
    
    if (src_offset <= dst_max) {
	uint64_t dst_offset;
	
	aal_assert("vpf-1005", dst_pos < tail40_units(dst));
	
	if (tail40_get_key(dst, dst_pos, &key))
	    return -EINVAL;
	
	dst_offset = plugin_call(key.plugin->o.key_ops, get_offset, &key);
	
	aal_assert("vpf-985", dst_offset < src_max);
	
	hint->src_count = (dst_max < src_max ? dst_max : src_max) - 
	    src_offset + 1;
	hint->len_delta = dst_offset - src_offset;
    } else {
	aal_assert("vpf-986", src_offset == dst_max + 1);

	hint->src_count = hint->len_delta = src_max - src_offset + 1;
    }
    
    plugin_call(hint->end.plugin->o.key_ops, set_offset, &hint->end, 
	src_offset + hint->src_count);
    
    return 0;
}


#endif
