/*
  extent40.c -- reiser4 default extent plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "extent40.h"
#include <aux/aux.h>

static reiser4_core_t *core = NULL;

static extent40_t *extent40_body(item_entity_t *item) {
	return (extent40_t *)item->body;
}

static uint32_t extent40_units(item_entity_t *item) {
	aal_assert("umka-1446", item != NULL, return 0);

	if (item->len % sizeof(extent40_t) != 0) {
		aal_exception_error("Invalid item size detected. Node %llu, "
				    "item %u.", item->con.blk, item->pos);
		return 0;
	}
		
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

static errno_t extent40_print(item_entity_t *item, aal_stream_t *stream,
			      uint16_t options) 
{
	uint32_t i, count;
	extent40_t *extent;
    
	aal_assert("umka-1205", item != NULL, return -1);
	aal_assert("umka-1206", stream != NULL, return -1);

	extent = extent40_body(item);
	count = extent40_units(item);

	aal_stream_format(stream, "[ ");
	
	for (i = 0; i < count; i++) {
		aal_stream_format(stream, "%llu(%llu)%s", et40_get_start(extent + i),
			       et40_get_width(extent + i), (i < count - 1 ? " " : ""));
	}
	
	aal_stream_format(stream, " ]");
    
	return 0;
}

static uint64_t extent40_size(item_entity_t *item) {
	uint32_t i;
	extent40_t *extent;
	uint32_t blocks = 0;
    
	aal_assert("umka-1583", item != NULL, return -1);

	extent = extent40_body(item);
	
	for (i = 0; i < extent40_units(item); i++)
		blocks += et40_get_width(extent + i);
    
	return (blocks * aal_device_get_bs(item->con.device));
}

#endif

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
	uint8_t i;
	uint64_t offset, delta, blocksize;
	
	aal_assert("vpf-437", item != NULL, return -1);
	aal_assert("vpf-438", key  != NULL, return -1);

	if (plugin_call(return -1, key->plugin->key_ops,
			assign, key->body, item->key.body))
		return -1;
			
	if ((offset = plugin_call(return -1, key->plugin->key_ops,
				  get_offset, key->body)))
		return -1;
	
	blocksize = item->con.device->blocksize;
	
	aal_assert("vpf-440", blocksize != 0, return -1);
	
	/* Key offset + for all units { width * blocksize } */
	for (i = 0; i < extent40_units(item); i++) {
		delta = et40_get_width(extent40_body(item) + i);
		
		aal_assert("vpf-439", delta < ((uint64_t)-1) / 102400, 
			   return -1);

		delta *= blocksize;
		
		aal_assert("vpf-503", offset < ((uint64_t)-1) - delta, 
			   return -1);
		
		offset += delta;
	}

	plugin_call(return -1, key->plugin->key_ops, set_offset, key->body, 
		    offset - 1);
	
	return 0;	
}

static errno_t extent40_unit_key(item_entity_t *item, uint16_t pos, 
	reiser4_key_t *key) 
{
	int i;
	uint16_t count;
	extent40_t *extent;
	uint64_t offset, blocksize;
	
	aal_assert("vpf-622", item != NULL, return -1);
	aal_assert("vpf-623", key != NULL, return -1);
	
	count = extent40_units(item);

	aal_assert("vpf-625", pos < count, return -1);
	
	extent = extent40_body(item);
	blocksize = item->con.device->blocksize;

	aal_memcpy(key, &item->key, sizeof(*key));
		
	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, key->body);

	for (i = 0; i < pos; i++)
		offset += et40_get_width(extent + i) * blocksize;

	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key->body, offset);
	
	return 0;
}

static int extent40_lookup(item_entity_t *item, reiser4_key_t *key,
			     uint32_t *pos)
{
	uint32_t i, count;
	uint32_t blocksize;
	
	extent40_t *extent;
	reiser4_key_t maxkey;
	uint64_t offset, lookuped;

	aal_assert("umka-1500", item != NULL, return -1);
	aal_assert("umka-1501", key  != NULL, return -1);
	aal_assert("umka-1502", pos != NULL, return -1);
	
	maxkey.plugin = key->plugin;

	if (extent40_max_poss_key(item, &maxkey))
		return -1;

	if (!(count = extent40_units(item)))
		return -1;
	
	if (plugin_call(return -1, key->plugin->key_ops,
			compare, key->body, maxkey.body) > 0)
	{
		*pos = count - 1;
		return 0;
	}

	lookuped = plugin_call(return -1, key->plugin->key_ops,
			       get_offset, key->body);

	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, item->key.body);

	extent = extent40_body(item);
	blocksize = item->con.device->blocksize;
		
	for (i = 0; i < count; i++, extent++) {
		offset += (blocksize * et40_get_width(extent));
		
		if (offset > lookuped) {
			*pos = i;
			return 1;
		}
	}

	*pos = count - 1;
	return 1;
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

static int extent40_mergeable(item_entity_t *item1, item_entity_t *item2) {
	reiser4_plugin_t *plugin;
	uint64_t offset1, offset2;
	roid_t objectid1, objectid2;
	roid_t locality1, locality2;
	
	aal_assert("umka-1581", item1 != NULL, return -1);
	aal_assert("umka-1582", item2 != NULL, return -1);

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

	if (offset1 + extent40_size(item1) != offset2)
		return 0;
	
	return 1;
}

#endif

static reiser4_plugin_t extent40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = ITEM_EXTENT40_ID,
			.group = EXTENT_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "extent40",
			.desc = "Extent item for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.init	       = extent40_init,
		.update        = extent40_update,
		.insert	       = extent40_insert,
		.remove	       = extent40_remove,
		.print	       = extent40_print,
		.mergeable     = extent40_mergeable,
#else
		.init	       = NULL,
		.update        = NULL,
		.insert	       = NULL,
		.remove	       = NULL,
		.print	       = NULL,
		.mergeable     = NULL, 
#endif
		.estimate      = NULL,
		.check	       = NULL,
		.valid	       = NULL,
		.open          = NULL,

		.shift         = NULL,
		.predict       = NULL,

		.lookup	       = extent40_lookup,
		.units	       = extent40_units,
		.fetch         = extent40_fetch,
		
		.max_poss_key = extent40_max_poss_key,
		.max_real_key = extent40_max_real_key,
		.unit_key     = extent40_unit_key,
	}
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
	core = c;
	return &extent40_plugin;
}

plugin_register(extent40_start, NULL);
