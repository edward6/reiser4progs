/*
    stat40_repair.c -- reiser4 default stat data plugin.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "stat40.h"
#include <repair/repair_plugin.h>

extern errno_t stat40_traverse(item_entity_t *, stat40_ext_func_t, void *);

struct pos_hint {
    sdext_entity_t sdext;
    uint8_t mode;
};

static errno_t callback_check_ext(sdext_entity_t *sdext, uint16_t extmask, 
    void *data) 
{    
    struct pos_hint *hint = (struct pos_hint *)data;
    
    hint->sdext = *sdext;
    
    return sdext->plugin->o.sdext_ops->check ? 
	sdext->plugin->o.sdext_ops->check(sdext, hint->mode) : REPAIR_OK;
}

errno_t stat40_check(item_entity_t *item, uint8_t mode) {
    struct pos_hint hint;
    errno_t res;
    
    aal_assert("vpf-775", item != NULL);
    
    aal_memset(&hint, 0, sizeof(struct pos_hint));
    
    res = stat40_traverse(item, callback_check_ext, &hint);

    if (res < 0) 
	return res;
    else if (res > 0 || !hint.sdext.plugin) {
	aal_exception_error("Node (%llu), item (%u): does not look like a "
	    "valid stat data.", item->context.blk, item->pos.item);
	
	return REPAIR_FATAL;
    }
    
    /* hint is set up by callback, so the last extention lenght has not been 
     * added yet. */
    hint.sdext.offset += plugin_call(hint.sdext.plugin->o.sdext_ops, length, 
	hint.sdext.body);
    
    aal_assert("vpf-784", hint.sdext.offset <= item->len);
    
    if (hint.sdext.offset < item->len) {
	aal_exception_error("Node (%llu), item (%u): item has a wrong length "
	    "(%u). Should be (%u). %s", item->context.blk, item->pos.item, 
	     item->len, hint.sdext.offset, mode == REPAIR_REBUILD ? 
	     "Fixed." : "");
	
	if (mode == REPAIR_REBUILD)
	    item->len = hint.sdext.offset;
	
	return mode == REPAIR_REBUILD ? REPAIR_FIXED : REPAIR_FATAL;
    }
    
    return REPAIR_OK;
}

errno_t stat40_copy(item_entity_t *dst, uint32_t dst_pos, 
    item_entity_t *src, uint32_t src_pos, copy_hint_t *hint) 
{
    aal_assert("vpf-979", dst  != NULL);
    aal_assert("vpf-980", src  != NULL);
    aal_assert("vpf-981", hint != NULL);
	
    aal_memcpy(dst->body, src->body, hint->len_delta);
    
    return 0;
}

errno_t stat40_estimate_copy(item_entity_t *dst, uint32_t dst_pos, 
    item_entity_t *src, uint32_t src_pos, copy_hint_t *hint)
{
    key_entity_t *key;
    
    aal_assert("vpf-969", dst  != NULL);
    aal_assert("vpf-970", src  != NULL);
    aal_assert("vpf-971", hint != NULL);
    
    hint->src_count = 1;
    hint->dst_count = 0;
    hint->len_delta = src->len - dst->len;
    
    key = plugin_call(hint->end.plugin->o.key_ops, maximal);
    
    plugin_call(hint->end.plugin->o.key_ops, assign, &hint->end, key);
    
    return 0;
}

#endif

