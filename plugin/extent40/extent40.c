/*
    extent40.c -- reiser4 default extent plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include "extent40.h"

static reiser4_core_t *core = NULL;

static extent40_t *extent40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return (extent40_t *)plugin_call(return NULL, item->node->plugin->node_ops, 
	item_body, item->node, item->pos);
}

#ifndef ENABLE_COMPACT

static errno_t extent40_init(reiser4_item_t *item, 
    reiser4_item_hint_t *hint)
{
    aal_assert("umka-1202", item != NULL, return -1); 
    aal_assert("umka-1203", hint != NULL, return -1);
    aal_assert("umka-1204", hint->data != NULL, return -1);
    
    return 0;
}

static errno_t extent40_insert(reiser4_item_t *item, uint32_t pos, 
    reiser4_item_hint_t *hint)
{
    return -1;
}

static uint16_t extent40_remove(reiser4_item_t *item, uint32_t pos) {
    return -1;
}

#endif

static errno_t extent40_print(reiser4_item_t *item, char *buff, 
    uint32_t n, uint16_t options) 
{
    extent40_t *extent;
    
    aal_assert("umka-1205", item != NULL, return -1);
    aal_assert("umka-1206", buff != NULL, return -1);

    extent = extent40_body(item);

    aal_snprintf(buff, n, "%llu(%llu)", et40_get_start(extent),
	et40_get_width(extent));
    
    return 0;
}

static errno_t extent40_max_poss_key(reiser4_item_t *item,
    reiser4_key_t *key) 
{
    uint64_t offset;
    reiser4_body_t *maxkey;
    
    aal_assert("umka-1211", item != NULL, return -1);
    aal_assert("umka-1212", key != NULL, return -1);

    if (plugin_call(return 0, item->node->plugin->node_ops,
	    get_key, item->node, item->pos, key))
	return -1;
    
    maxkey = plugin_call(return -1, key->plugin->key_ops,
	maximal,);
    
    offset = plugin_call(return -1, key->plugin->key_ops,
	get_offset, maxkey);
    
    plugin_call(return -1, key->plugin->key_ops, set_offset, 
	key->body, offset);

    return 0;
}

static errno_t extent40_max_real_key(reiser4_item_t *item,
    reiser4_key_t *key) 
{
    return 0;
}

static reiser4_plugin_t extent40_plugin = {
    .item_ops = {
	.h = {
	    .handle = NULL,
	    .id = ITEM_EXTENT40_ID,
	    .type = ITEM_PLUGIN_TYPE,
	    .label = "extent40",
	    .desc = "Extent item for reiserfs 4.0, ver. " VERSION,
	},
	.type = EXTENT_ITEM_TYPE,
	
#ifndef ENABLE_COMPACT
        .init		= extent40_init,
        .insert		= extent40_insert,
        .remove		= extent40_remove,
#else
        .init		= NULL,
        .insert		= NULL,
        .remove		= NULL,
#endif
        .estimate	= NULL,
        .check		= NULL,
        .lookup		= NULL,
        .count		= NULL,
        .valid		= NULL,
        .max_poss_key	= extent40_max_poss_key,
        .max_real_key   = extent40_max_real_key,
        .print		= extent40_print,

	.specific	= {}
    }
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
    core = c;
    return &extent40_plugin;
}

plugin_register(extent40_start);

