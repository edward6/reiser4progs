/*
  tail40.c -- reiser4 default tail plugin.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aux/aux.h>
#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

static reiser4_body_t *tail40_body(item_entity_t *item) {
	return item->body;
}

#ifndef ENABLE_COMPACT

static errno_t tail40_insert(item_entity_t *item, uint32_t pos, 
			     reiser4_item_hint_t *hint)
{
	aal_assert("umka-1172", item != NULL, return -1); 
	aal_assert("umka-1173", hint != NULL, return -1);
	aal_assert("umka-1178", hint->data != NULL, return -1);
    
	aal_memcpy(tail40_body(item) + pos, hint->data, hint->len);
	return 0;
}

static errno_t tail40_init(item_entity_t *item, 
			   reiser4_item_hint_t *hint)
{
	return tail40_insert(item, 0, hint);
}

static errno_t tail40_print(item_entity_t *item, aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1489", item != NULL, return -1);
	aal_assert("umka-1490", stream != NULL, return -1);

	aal_stream_format(stream, "len:\t\t%u\n", item->len);
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
	uint64_t offset;

	aal_assert("vpf-442", item != NULL, return -1);
	aal_assert("vpf-443", key != NULL, return -1);

	if (plugin_call(return -1, key->plugin->key_ops,
			assign, key->body, item->key.body))
		return -1;

	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, key);
	
	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		key, offset + item->len - 1);
	
	return 0;
}

static uint32_t tail40_units(item_entity_t *item) {
	return item->len;
}

static errno_t tail40_unit_key(item_entity_t *item, uint16_t pos, 
	reiser4_key_t *key) 
{
	uint64_t offset;
	uint16_t count;

	aal_assert("vpf-626", item != NULL, return -1);
	aal_assert("vpf-627", key != NULL, return -1);

	count = tail40_count(item);

	aal_assert("vpf-628", pos < count, return -1);
	
	aal_memcpy(key, &item->key, sizeof(*key));

	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, key->body);

	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key->body, offset + pos);

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

#ifndef ENABLE_COMPACT

static int tail40_mergeable(item_entity_t *item1, item_entity_t *item2) {
	reiser4_plugin_t *plugin;
	uint64_t offset1, offset2;
	roid_t objectid1, objectid2;
	roid_t locality1, locality2;
	
	aal_assert("umka-1584", item1 != NULL, return -1);
	aal_assert("umka-1585", item2 != NULL, return -1);

	/* FIXME-UMKA: Here should not be hardcoded key plugin id */
	if (!(plugin = core->factory_ops.ifind(KEY_PLUGIN_TYPE,
					       KEY_REISER40_ID)))
	{
		aal_exception_error("Can't find key plugin by its id 0x%x",
				    KEY_REISER40_ID);
		return -1;
	}
	
	locality1 = plugin_call(return -1, plugin->key_ops,
				get_locality, &item1->key);

	locality2 = plugin_call(return -1, plugin->key_ops,
				get_locality, &item2->key);

	if (locality1 != locality2)
		return 0;
	
	objectid1 = plugin_call(return -1, plugin->key_ops,
				get_objectid, &item1->key);
	
	objectid2 = plugin_call(return -1, plugin->key_ops,
				get_objectid, &item2->key);

	if (objectid1 != objectid1)
		return 0;

	offset1 = plugin_call(return -1, plugin->key_ops,
			      get_offset, &item1->key);
	
	offset2 = plugin_call(return -1, plugin->key_ops,
			      get_offset, &item2->key);

	if (offset1 + item1->len != offset2)
		return 0;
	
	return 1;
}

#endif

static reiser4_plugin_t tail40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = ITEM_TAIL40_ID,
			.group = TAIL_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "tail40",
			.desc = "Tail item for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.init	       = tail40_init,
		.insert	       = tail40_insert,
		.print	       = tail40_print,
		.mergeable     = tail40_mergeable,
#else
		.init	       = NULL,
		.insert	       = NULL,
		.print	       = NULL,
		.mergeable     = NULL,
#endif
		.open          = NULL,
		.remove	       = NULL,
		.estimate      = NULL,
		.check	       = NULL,
		.valid	       = NULL,
		.update        = NULL,

		.shift         = NULL,
		.predict       = NULL,
		
		.units	       = tail40_units,
		.lookup	       = tail40_lookup,
		.fetch         = tail40_fetch,
		
		.max_poss_key  = tail40_max_poss_key,
		.max_real_key  = tail40_max_real_key,
		.unit_key      = tail40_unit_key,
	}
};

static reiser4_plugin_t *tail40_start(reiser4_core_t *c) {
	core = c;
	return &tail40_plugin;
}

plugin_register(tail40_start, NULL);

