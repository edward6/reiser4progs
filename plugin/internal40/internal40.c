/*
    internal40.c -- reiser4 default internal item plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include "internal40.h"

static reiser4_core_t *core = NULL;

static internal40_t *internal40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return (internal40_t *)plugin_call(return NULL, 
	item->node->plugin->node_ops, item_body, item->node, item->pos);
}

#ifndef ENABLE_COMPACT

static errno_t internal40_init(reiser4_item_t *item, 
    reiser4_item_hint_t *hint)
{
    internal40_t *internal;
    
    aal_assert("vpf-063", item != NULL, return -1); 
    aal_assert("vpf-064", hint != NULL, return -1);

    internal = internal40_body(item);
    
    it40_set_ptr(internal, 
	((reiser4_internal_hint_t *)hint->hint)->ptr);
	    
    return 0;
}

static errno_t internal40_estimate(reiser4_item_t *item,
    uint32_t pos, reiser4_item_hint_t *hint) 
{
    aal_assert("vpf-068", hint != NULL, return -1);
    
    hint->len = sizeof(internal40_t);
    return 0;
}

extern errno_t internal40_check(reiser4_item_t *item, 
    uint16_t options);

#endif

static errno_t internal40_print(reiser4_item_t *item, 
    char *buff, uint32_t n, uint16_t options) 
{
    aal_assert("umka-544", item != NULL, return -1);
    aal_assert("umka-545", buff != NULL, return -1);

    return -1;
}

#ifndef ENABLE_COMPACT

static errno_t internal40_set_ptr(reiser4_item_t *item, 
    blk_t blk)
{
    internal40_t *internal;
    
    aal_assert("umka-605", item != NULL, return -1);

    internal = internal40_body(item);
    it40_set_ptr(internal, blk);

    return 0;
}

#endif

static blk_t internal40_get_ptr(reiser4_item_t *item) {
    internal40_t *internal;
    
    aal_assert("umka-606", item != NULL, return 0);
    internal = internal40_body(item);
    
    return it40_get_ptr(internal);
}

static errno_t internal40_max_poss_key(reiser4_item_t *item,
    reiser4_key_t *key) 
{
    aal_assert("umka-1207", item != NULL, return -1);
    aal_assert("umka-1208", key != NULL, return -1);

    return plugin_call(return 0, item->node->plugin->node_ops,
	get_key, item->node, item->pos, key);
}

static reiser4_plugin_t internal40_plugin = {
    .item_ops = {
	.h = {
    	    .handle = NULL,
	    .id = ITEM_INTERNAL40_ID,
	    .type = ITEM_PLUGIN_TYPE,
	    .label = "internal40",
	    .desc = "Internal item for reiserfs 4.0, ver. " VERSION,
	},
	.type = INTERNAL_ITEM_TYPE,
	
#ifndef ENABLE_COMPACT	    
        .init		= internal40_init,
        .estimate	= internal40_estimate,
        .check		= internal40_check,
#else
        .init		= NULL,
        .estimate	= NULL,
        .check		= NULL,
#endif
        .lookup		= NULL,
        .valid		= NULL,
        .insert		= NULL,
        .count		= NULL,
        .remove		= NULL,

	.max_poss_key	= internal40_max_poss_key,
        .print		= internal40_print,
	.max_real_key   = internal40_maxkey,
	
    	.specific = {
	    .internal = {
		.get_ptr = internal40_get_ptr,
#ifndef ENABLE_COMPACT
		.set_ptr = internal40_set_ptr
#else
		.set_ptr = NULL
#endif
	    }
	}
    }
};

static reiser4_plugin_t *internal40_start(reiser4_core_t *c) {
    core = c;
    return &internal40_plugin;
}

plugin_register(internal40_start);
