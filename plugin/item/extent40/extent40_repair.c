/*
  extent40_repare.c -- repair dafault extent plugin methods.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "extent40.h"
#include <repair/repair_plugin.h>

extern uint32_t extent40_units(item_entity_t *item);
extern errno_t extent40_maxreal_key(item_entity_t *item, key_entity_t *key);
extern uint32_t extent40_blocksize(item_entity_t *item);
extern errno_t extent40_maxposs_key(item_entity_t *item, key_entity_t *key);
extern uint64_t extent40_offset(item_entity_t *item, uint64_t pos);
extern uint32_t extent40_unit(item_entity_t *item, uint64_t offset);
extern lookup_t extent40_lookup(item_entity_t *item, key_entity_t *key, 
    uint32_t *pos);

errno_t extent40_layout_check(item_entity_t *item, region_func_t func, 
    void *data, uint8_t mode) 
{
    uint32_t i, units;
    extent40_t *extent;
    errno_t res, result = REPAIR_OK;
	
    aal_assert("vpf-724", item != NULL);
    aal_assert("vpf-725", func != NULL);

    extent = extent40_body(item);
    units = extent40_units(item);
			
    for (i = 0; i < units; i++, extent++) {
	uint64_t start, width;

	start = et40_get_start(extent);
	width = et40_get_width(extent);

	if (start) {
	    res = func(item, start, width, data);

	    if (res > 0) {
		if (mode == REPAIR_CHECK)
		    result = REPAIR_FIXABLE;
		else {
		    /* Zero the problem region. */
		    aal_exception_error("Node (%llu), item (%u): pointed "
			"region [%llu..%llu] is zeroed.", item->context.blk, 
			item->pos.item, start, start + width - 1);
		    et40_set_start(extent, 0);
		    result = REPAIR_FIXED;
		}
	    } else if (res < 0) 
		return res;
	}
    }

    return result;
}

errno_t extent40_check(item_entity_t *item, uint8_t mode) {
    aal_assert("vpf-750", item != NULL);
    return item->len % sizeof(extent40_t) ? REPAIR_FATAL : REPAIR_OK;
}

errno_t extent40_copy(item_entity_t *dst, uint32_t dst_pos, 
    item_entity_t *src, uint32_t src_pos, copy_hint_t *hint)
{
    aal_assert("vpf-993", hint != NULL);
    aal_assert("vpf-994", dst  != NULL);
    aal_assert("vpf-995", src  != NULL);

    return 0;
}

/* FIXME-VITALY: Do not forget to handle the case with unit's @start == 0. */
errno_t extent40_feel_copy(item_entity_t *dst, uint32_t dst_pos, 
    item_entity_t *src, uint32_t src_pos, copy_hint_t *hint)
{
    uint64_t dst_max, src_min, src_max, src_end;
    uint64_t src_start, dst_start, dst_min;
    extent40_t *dst_body, *src_body;
    key_entity_t key;
    lookup_t lookup;
    uint64_t b_size;
    uint32_t pos;
    errno_t res;
    
    aal_assert("vpf-990", hint != NULL);
    aal_assert("vpf-991", dst  != NULL);
    aal_assert("vpf-992", src  != NULL);
    
    b_size = extent40_blocksize(src);
    
    dst_body = extent40_body(dst);
    src_body = extent40_body(src);
    
    /* Getting src_start, dst_start, src_max, dst_max, dst_min and src_min. */
    src_end = plugin_call(hint->end.plugin->o.key_ops, get_offset, &hint->end) + 1;
    src_start = plugin_call(hint->start.plugin->o.key_ops, get_offset, &hint->start);

    src_min = plugin_call(src->key.plugin->o.key_ops, get_offset, &src->key);
    
    if ((res = extent40_maxposs_key(src, &key)))
	return res;

    src_max = plugin_call(key.plugin->o.key_ops, get_offset, &key) + 1;
    
    /* Copy through src_end only. */
    if (src_max > src_end)
	src_max = src_end;
	
    aal_assert("vpf-996", src_start % b_size == 0);
    aal_assert("vpf-998", src_max % b_size == 0);
    aal_assert("vpf-999", dst_max % b_size == 0);
    aal_assert("vpf-1009", src_start < src_max);
    
    hint->src_head = (src_start - extent40_offset(src, src_pos) - src_min) / b_size;
    hint->src_tail = (src_max - extent40_offset(src, src_pos) - src_min) / b_size;

    if ((res = extent40_maxposs_key(dst, &key)))
	return res;
    
    dst_max = plugin_call(key.plugin->o.key_ops, get_offset, &key) + 1;
    
    if (dst_pos >= extent40_units(dst)) {
	aal_assert("vpf-1007", src_start == dst_max);
	
	hint->dst_count = 0;
	hint->src_count = extent40_unit(src, src_max - 1 - src_min) - src_pos + 1;
	hint->len_delta = sizeof(extent40_t) * hint->src_count;
	
	hint->dst_head = hint->dst_tail = 0;
		
	plugin_call(hint->end.plugin->o.key_ops, set_offset, &hint->end, src_max);
	
	return 0;
    }
    
    dst_min = plugin_call(dst->key.plugin->o.key_ops, get_offset, &dst->key);
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
	
    plugin_call(key.plugin->o.key_ops, set_offset, &key, src_max - 1);    
    plugin_call(hint->end.plugin->o.key_ops, set_offset, &hint->end, src_max);
    
    lookup = extent40_lookup(dst, &key, &pos);
    aal_assert("vpf-1001", lookup == LP_PRESENT);
	
    hint->dst_count = pos - dst_pos + 1;
    hint->src_count = extent40_unit(src, src_max - 1 - src_min) - src_pos + 1;
    hint->dst_tail = (src_max - extent40_offset(dst, pos) - dst_min) / b_size;

    if (hint->dst_tail == et40_get_width(dst_body + pos))
	hint->tail = 0;
    else if(et40_get_start(src_body + src_pos + hint->src_count - 1) + 
	    hint->src_head == 
	    et40_get_start(dst_body + pos) + hint->dst_head)
	hint->tail = 0;
    else
	hint->tail = 1;
    
    hint->len_delta = hint->src_count - hint->dst_count;
    
    if (hint->head)
	hint->len_delta++;
    
    if (hint->tail)
	hint->len_delta++;

    hint->len_delta *= sizeof(extent40_t);
    return 0;
}

#endif
