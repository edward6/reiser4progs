/*
    nodeptr40.c -- reiser4 default node pointer item plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include "nodeptr40.h"

static reiser4_core_t *core = NULL;

static nodeptr40_t *nodeptr40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return (nodeptr40_t *)plugin_call(return NULL, 
	item->node->plugin->node_ops, item_body, item->node, item->pos);
}

static uint32_t nodeptr40_count(reiser4_item_t *item) {
    return 1;
}

#ifndef ENABLE_COMPACT

static errno_t nodeptr40_init(reiser4_item_t *item, 
    reiser4_item_hint_t *hint)
{
    nodeptr40_t *internal;
    
    aal_assert("vpf-063", item != NULL, return -1); 
    aal_assert("vpf-064", hint != NULL, return -1);

    internal = nodeptr40_body(item);
    
    np40_set_ptr(internal, 
	((reiser4_internal_hint_t *)hint->hint)->ptr);
	    
    return 0;
}

static errno_t nodeptr40_estimate(reiser4_item_t *item,
    uint32_t pos, reiser4_item_hint_t *hint) 
{
    aal_assert("vpf-068", hint != NULL, return -1);
    
    hint->len = sizeof(nodeptr40_t);
    return 0;
}

static errno_t nodeptr40_set_ptr(reiser4_item_t *item, uint64_t ptr) {
    nodeptr40_t *internal;
    
    aal_assert("umka-605", item != NULL, return -1);
    aal_assert("vpf-361", item->pos != NULL, return -1);
    
    internal = nodeptr40_body(item);
    np40_set_ptr(internal, ptr);

    return 0;
}

extern errno_t nodeptr40_check(reiser4_item_t *item, 
    uint16_t options);

#endif

static uint64_t nodeptr40_get_ptr(reiser4_item_t *item) {
    nodeptr40_t *internal;
    
    aal_assert("umka-606", item != NULL, return FAKE_BLK);
    aal_assert("vpf-362", item->pos != NULL, return FAKE_BLK);
    
    internal = nodeptr40_body(item);
    
    return np40_get_ptr(internal);
}

static errno_t nodeptr40_print(reiser4_item_t *item, 
    char *buff, uint32_t n, uint16_t options) 
{
    aal_assert("umka-544", item != NULL, return -1);
    aal_assert("umka-545", buff != NULL, return -1);
    
    aal_snprintf(buff, n, "%llu", nodeptr40_get_ptr(item));
    return 0;
}

static errno_t nodeptr40_max_poss_key(reiser4_item_t *item,
    reiser4_key_t *key) 
{
    aal_assert("umka-1207", item != NULL, return -1);
    aal_assert("umka-1208", key != NULL, return -1);

    return plugin_call(return 0, item->node->plugin->node_ops,
	get_key, item->node, item->pos, key);
}

static reiser4_plugin_t nodeptr40_plugin = {
    .item_ops = {
	.h = {
    	    .handle = NULL,
	    .id = ITEM_NODEPTR40_ID,
	    .group = NODEPTR_ITEM,
	    .type = ITEM_PLUGIN_TYPE,
	    .label = "nodeptr40",
	    .desc = "Internal item for reiserfs 4.0, ver. " VERSION,
	},
#ifndef ENABLE_COMPACT	    
        .init		= nodeptr40_init,
        .estimate	= nodeptr40_estimate,
        .check		= nodeptr40_check,
#else
        .init		= NULL,
        .estimate	= NULL,
        .check		= NULL,
#endif
        .lookup		= NULL,
        .valid		= NULL,
        .insert		= NULL,
        .remove		= NULL,
	.detect		= NULL,

        .print		= nodeptr40_print,
        .count		= nodeptr40_count,
	
	.max_poss_key	= nodeptr40_max_poss_key,
	.max_real_key   = nodeptr40_max_poss_key,
	
    	.specific = {
	    .ptr = {
		.get_ptr    = nodeptr40_get_ptr,
#ifndef ENABLE_COMPACT
		.set_ptr    = nodeptr40_set_ptr,
#else
		.set_ptr    = NULL,
#endif
		.get_width  = NULL,
		.set_width  = NULL
	    }
	}
    }
};

static reiser4_plugin_t *nodeptr40_start(reiser4_core_t *c) {
    core = c;
    return &nodeptr40_plugin;
}

plugin_register(nodeptr40_start);
