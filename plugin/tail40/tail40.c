/*
  tail40.c -- reiser4 default tail plugin.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

static reiser4_body_t *tail40_body(item_entity_t *item) {
	return item->body;
}

#ifndef ENABLE_COMPACT

static errno_t tail40_init(item_entity_t *item, 
			   reiser4_item_hint_t *hint)
{
	aal_assert("umka-1172", item != NULL, return -1); 
	aal_assert("umka-1173", hint != NULL, return -1);
	aal_assert("umka-1178", hint->data != NULL, return -1);
    
	aal_memcpy(tail40_body(item), hint->data, hint->len);
	return 0;
}

static errno_t tail40_insert(item_entity_t *item, uint32_t pos, 
			     reiser4_item_hint_t *hint)
{
	aal_memcpy(tail40_body(item) + pos, hint->data, hint->len);
	return 0;
}

#endif

static errno_t tail40_max_poss_key(item_entity_t *item,
				   reiser4_key_t *key) 
{
	uint64_t offset;
	reiser4_body_t *maxkey;
    
	aal_assert("umka-1209", item != NULL, return -1);
	aal_assert("umka-1210", key != NULL, return -1);


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

static errno_t tail40_max_real_key(item_entity_t *item,
				   reiser4_key_t *key) 
{
	return 0;
}

static errno_t tail40_fetch(item_entity_t *item, uint32_t pos,
			    void *buff, uint32_t count)
{
	aal_memcpy(buff, tail40_body(item) + pos, count);
	return 0;
}

static int tail40_lookup(item_entity_t *item, reiser4_key_t *key, 
			 uint32_t *pos)
{
	uint32_t cur_offset;
	uint32_t wan_offset;
    
	reiser4_key_t curkey;
	reiser4_key_t maxkey;
    
	aal_assert("umka-1228", item != NULL, return -1);
	aal_assert("umka-1229", key != NULL, return -1);
	aal_assert("umka-1230", pos != NULL, return -1);

	maxkey.plugin = key->plugin;
	tail40_max_poss_key(item, &maxkey);

	if (plugin_call(return -1, key->plugin->key_ops, compare,
			key->body, maxkey.body))
	{
		*pos = item->len;
		return 0;
	}

	curkey.plugin = key->plugin;
	if (plugin_call(return -1, curkey.plugin->key_ops,
			assign, curkey.body, item->key.body))
		return -1;

	cur_offset = plugin_call(return -1, key->plugin->key_ops,
				 get_offset, curkey.body);
    
	wan_offset = plugin_call(return -1, key->plugin->key_ops,
				 get_offset, key->body);
    
	if (wan_offset >= cur_offset && wan_offset < cur_offset + item->len) {
		*pos = wan_offset - cur_offset;
		return 1;
	}

	*pos = item->len;
	return 0;
}

static reiser4_plugin_t tail40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = ITEM_TAIL40_ID,
				.group = TAIL_ITEM,
				.type = ITEM_PLUGIN_TYPE
			},
			.label = "tail40",
			.desc = "Tail item for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.init	       = tail40_init,
		.insert	       = tail40_insert,
		.print	       = NULL,
#else
		.init	       = NULL,
		.insert	       = NULL,
		.print	       = NULL,
#endif
		.open          = NULL,
		.remove	       = NULL,
		.estimate      = NULL,
		.check	       = NULL,
		.count	       = NULL,
		.valid	       = NULL,
		.shift         = NULL,
		.update        = NULL,
		
		.lookup	       = tail40_lookup,
		.fetch         = tail40_fetch,
		
		.max_poss_key  = tail40_max_poss_key,
		.max_real_key  = tail40_max_real_key,
	}
};

static reiser4_plugin_t *tail40_start(reiser4_core_t *c) {
	core = c;
	return &tail40_plugin;
}

plugin_register(tail40_start, NULL);

