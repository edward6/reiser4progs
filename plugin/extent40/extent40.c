/*
  extent40.c -- reiser4 default extent plugin.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#include "extent40.h"

static reiser4_core_t *core = NULL;

static extent40_t *extent40_body(item_entity_t *item) {
	return (extent40_t *)item->body;
}

static uint32_t extent40_count(item_entity_t *item) {
	aal_assert("umka-1446", item != NULL, return 0);
	return item->len / sizeof(extent40_t);
}

#ifndef ENABLE_COMPACT

static errno_t extent40_init(item_entity_t *item, 
			     reiser4_item_hint_t *hint)
{
	aal_assert("umka-1202", item != NULL, return -1); 
	aal_assert("umka-1203", hint != NULL, return -1);
	aal_assert("umka-1204", hint->data != NULL, return -1);
    
	return 0;
}

static errno_t extent40_insert(item_entity_t *item, uint32_t pos, 
			       reiser4_item_hint_t *hint)
{
	return -1;
}

static uint16_t extent40_remove(item_entity_t *item, uint32_t pos) {
	return -1;
}

#endif

static errno_t extent40_print(item_entity_t *item, char *buff, 
			      uint32_t n, uint16_t options) 
{
	extent40_t *extent;
	uint16_t i, count;
    
	aal_assert("umka-1205", item != NULL, return -1);
	aal_assert("umka-1206", buff != NULL, return -1);

	extent = extent40_body(item);
	count = extent40_count(item);

	for (i = 0; i < count; i++) {
		int len = aal_snprintf(buff, n, "%llu( %llu )%s", et40_get_start(extent + i),
				       et40_get_width(extent + i), (i < count - 1 ? ", " : ""));
		buff += len;
	}
    
	return 0;
}

static errno_t extent40_max_poss_key(item_entity_t *item,
				     reiser4_key_t *key) 
{
	uint64_t offset;
	reiser4_body_t *maxkey;
    
	aal_assert("umka-1211", item != NULL, return -1);
	aal_assert("umka-1212", key != NULL, return -1);

	if (plugin_call(return -1, key->plugin->key_ops,
			assign, key->body, item->key.body))
		return -1;
    
	maxkey = plugin_call(return -1, key->plugin->key_ops,
			     maximal,);
    
	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, maxkey);
    
	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key->body, offset);

	return 0;
}

static errno_t extent40_max_real_key(item_entity_t *item,
				     reiser4_key_t *key) 
{
	return 0;
}

static errno_t extent40_fetch(item_entity_t *item, uint32_t pos,
			      void *buff, uint32_t count)
{
	reiser4_ptr_hint_t *hint = (reiser4_ptr_hint_t *)buff;
	
	aal_assert("umka-1421", item != NULL, return -1);
	aal_assert("umka-1422", buff != NULL, return -1);

	hint->ptr = et40_get_start(extent40_body(item) + pos);
	hint->width = et40_get_width(extent40_body(item) + pos);
	
	return 0;
}

#ifndef ENABLE_COMPACT

static errno_t extent40_update(item_entity_t *item, uint32_t pos,
			       void *buff, uint32_t count)
{
	reiser4_ptr_hint_t *hint = (reiser4_ptr_hint_t *)buff;
	
	aal_assert("umka-1425", item != NULL, return -1);
	aal_assert("umka-1426", buff != NULL, return -1);

	et40_set_start(extent40_body(item) + pos, hint->ptr);
	et40_set_width(extent40_body(item) + pos, hint->width);
	
	return 0;
}

#endif

static reiser4_plugin_t extent40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = ITEM_EXTENT40_ID,
				.group = EXTENT_ITEM,
				.type = ITEM_PLUGIN_TYPE
			},
			.label = "extent40",
			.desc = "Extent item for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.init	       = extent40_init,
		.update        = extent40_update,
		.insert	       = extent40_insert,
		.remove	       = extent40_remove,
#else
		.init	       = NULL,
		.update        = NULL,
		.insert	       = NULL,
		.remove	       = NULL,
#endif
		.estimate      = NULL,
		.check	       = NULL,
		.lookup	       = NULL,
		.valid	       = NULL,
		.shift         = NULL,
		.open          = NULL,

		.count	       = extent40_count,
		.print	       = extent40_print,
		.fetch         = extent40_fetch,
		
		.max_poss_key = extent40_max_poss_key,
		.max_real_key = extent40_max_real_key,
	}
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
	core = c;
	return &extent40_plugin;
}

plugin_register(extent40_start, NULL);
