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

static uint32_t extent40_count(reiser4_item_t *item) {

    if (item == NULL) 
	return 0;
    
    return plugin_call(return 0, item->node->plugin->node_ops, 
	item_len, item->node, item->pos) / sizeof(extent40_t);
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
    uint16_t i, count;
    
    aal_assert("umka-1205", item != NULL, return -1);
    aal_assert("umka-1206", buff != NULL, return -1);

    extent = extent40_body(item);
    count = extent40_count(item);

    for (i = 0; i < count; i++) {
	aal_snprintf(buff, n, "%llu(%llu)", et40_get_start(extent + i),
	    et40_get_width(extent + i));
    }
    
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

#ifndef ENABLE_COMPACT

static errno_t extent40_set_ptr(reiser4_item_t *item, uint64_t ptr) {
    aal_assert("vpf-355", item != NULL, return -1);
    aal_assert("vpf-359", item->pos != NULL, return -1);

    aal_assert("vpf-356", 
	item->pos->unit < extent40_count(item), return -1);
    
    et40_set_start(extent40_body(item) + item->pos->unit, ptr);

    return 0;
}

static errno_t extent40_set_width(reiser4_item_t *item, 
    uint64_t width) 
{
    aal_assert("vpf-364", item != NULL, return -1);
    aal_assert("vpf-365", item->pos != NULL, return -1);
    
    aal_assert("vpf-366", 
	item->pos->unit < extent40_count(item), return -1);

    et40_set_width(extent40_body(item) + item->pos->unit, width);

    return 0;
}

#endif

static uint64_t extent40_get_ptr(reiser4_item_t *item) {
    aal_assert("vpf-357", item != NULL, return 0);
    aal_assert("vpf-367", item->pos != NULL, return 0);

    aal_assert("vpf-358", 
	item->pos->unit < extent40_count(item), return 0);

    return et40_get_start(extent40_body(item) + item->pos->unit);
}

static uint64_t extent40_get_width(reiser4_item_t *item) {    
    aal_assert("vpf-364", item != NULL, return FAKE_BLK);
    aal_assert("vpf-365", item->pos != NULL, return FAKE_BLK);

    aal_assert("vpf-366", 
	item->pos->unit < extent40_count(item), return FAKE_BLK);

    return et40_get_width(extent40_body(item) + item->pos->unit);
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
        .valid		= NULL,
	.detect		= NULL,

        .count		= extent40_count,
        .max_poss_key	= extent40_max_poss_key,
        .max_real_key   = extent40_max_real_key,
        .print		= extent40_print,
	
	.specific = {
	    .ptr = {
		.get_ptr    = extent40_get_ptr,
		.get_width  = extent40_get_width,
#ifndef ENABLE_COMPACT
		.set_ptr    = extent40_set_ptr,
		.set_width  = extent40_set_width
#else
		.set_ptr    = NULL,
		.set_width  = NULL
#endif
	    }
	}
    }
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
    core = c;
    return &extent40_plugin;
}

plugin_register(extent40_start);

