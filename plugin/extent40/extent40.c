/*
    extent40.c -- reiser4 default extent plugin.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

static reiser4_body_t *extent40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return plugin_call(return NULL, item->node->plugin->node_ops, 
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
        .init	    = extent40_init,
        .insert	    = extent40_insert,
        .remove	    = extent40_remove,
#else
        .init	    = NULL,
        .insert	    = NULL,
        .remove	    = NULL,
#endif
        .estimate   = NULL,
        .check	    = NULL,
        .maxkey	    = NULL,
        .lookup	    = NULL,
        .count	    = NULL,
        .valid	    = NULL,
        .print	    = NULL,

	.specific   = {}
    }
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
    core = c;
    return &extent40_plugin;
}

plugin_register(extent40_start);

