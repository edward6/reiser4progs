/*
    tail40.c -- reiser4 default tail plugin.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

static reiser4_body_t *tail40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return plugin_call(return NULL, item->node->plugin->node_ops, 
	item_body, item->node, item->pos);
}

#ifndef ENABLE_COMPACT

static errno_t tail40_init(reiser4_item_t *item, 
    reiser4_item_hint_t *hint)
{
    aal_assert("umka-1172", item != NULL, return -1); 
    aal_assert("umka-1173", hint != NULL, return -1);
    aal_assert("umka-1178", hint->data != NULL, return -1);
    
    aal_memcpy(tail40_body(item), hint->data, hint->len);
    return 0;
}

static errno_t tail40_insert(reiser4_item_t *item, uint32_t pos, 
    reiser4_item_hint_t *hint)
{
    return -1;
}

static uint16_t tail40_remove(reiser4_item_t *item, uint32_t pos) {
    return -1;
}

#endif

static reiser4_plugin_t tail40_plugin = {
    .item_ops = {
	.h = {
	    .handle = NULL,
	    .id = ITEM_TAIL40_ID,
	    .type = ITEM_PLUGIN_TYPE,
	    .label = "tail40",
	    .desc = "Tail item for reiserfs 4.0, ver. " VERSION,
	},
	.type = TAIL_ITEM_TYPE,
	
#ifndef ENABLE_COMPACT
        .init	    = tail40_init,
        .insert	    = tail40_insert,
        .remove	    = tail40_remove,
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

static reiser4_plugin_t *tail40_start(reiser4_core_t *c) {
    core = c;
    return &tail40_plugin;
}

plugin_register(tail40_start);

