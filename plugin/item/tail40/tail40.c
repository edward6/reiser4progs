/*
  tail40.c -- reiser4 default tail plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aux/aux.h>
#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;

#define tail40_body(item) (item->body)

/* Returns tail length */
static uint32_t tail40_units(item_entity_t *item) {
	return item->len;
}

/* Returns the key of the specified unit */
static errno_t tail40_get_key(item_entity_t *item,
			      uint32_t offset, 
			      key_entity_t *key) 
{
	uint32_t units;

	aal_assert("vpf-626", item != NULL);
	aal_assert("vpf-627", key != NULL);

	units = tail40_units(item);
	aal_assert("vpf-628", offset < units);
	
	aal_memcpy(key, &item->key, sizeof(*key));

	offset += plugin_call(key->plugin->key_ops,
			      get_offset, key);
	
	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset);

	return 0;
}

static int32_t tail40_read(item_entity_t *item, void *buff,
			   uint32_t pos, uint32_t count)
{
	aal_assert("umka-1673", item != NULL);
	aal_assert("umka-1674", buff != NULL);
	aal_assert("umka-1675", pos < item->len);

	if (count > item->len - pos)
		count = item->len - pos;
	
	aal_memcpy(buff, item->body + pos, count);
	
	return count;
}

static int tail40_data(void) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE

/* Rewrites tail from passed @pos by data specifed by hint */
static int32_t tail40_write(item_entity_t *item, void *buff,
			    uint32_t pos, uint32_t count)
{
	create_hint_t *hint;
	
	aal_assert("umka-1677", buff != NULL);
	aal_assert("umka-1678", item != NULL);
	aal_assert("umka-1679", pos < item->len);

	hint = (create_hint_t *)buff;
	
	if (count > item->len - pos)
		count = item->len - pos;
	
	/* Copying new data into place */
	aal_memcpy(item->body + pos, hint->type_specific, count);

	/* Updating the key */
	if (pos == 0) {
		if (tail40_get_key(item, 0, &item->key))
			return -EINVAL;
	}

	return count;
}

/* Removes the part of tail body */
static int32_t tail40_remove(item_entity_t *item, uint32_t pos,
			     uint32_t count)
{
	void *src, *dst;
	
	aal_assert("umka-1661", item != NULL);
	aal_assert("umka-1663", pos < item->len);

	if (count > item->len - pos)
		count = item->len - pos;

	if (pos + count < item->len - 1) {
		src = item->body + pos;
		dst = item->body + pos + count;
		aal_memmove(dst, src, item->len - (pos + count));
	}

	/* Updating the key */
	if (pos == 0) {
		if (tail40_get_key(item, 0, &item->key))
			return -EINVAL;
	}
	
	return count;
}

static errno_t tail40_init(item_entity_t *item) {
	aal_assert("umka-1668", item != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

static errno_t tail40_print(item_entity_t *item,
			    aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1489", item != NULL);
	aal_assert("umka-1490", stream != NULL);

	aal_stream_format(stream, "TAIL PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->h.label, item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=%u\n", item->len);
	
	return 0;
}

#endif

static errno_t tail40_maxposs_key(item_entity_t *item,
				  key_entity_t *key) 
{
	uint64_t offset;
	key_entity_t *maxkey;
    
	aal_assert("umka-1209", item != NULL);
	aal_assert("umka-1210", key != NULL);

	key->plugin = item->key.plugin;
		
	if (plugin_call(key->plugin->key_ops, assign, key, &item->key))
		return -EINVAL;
	
	maxkey = plugin_call(key->plugin->key_ops,
			     maximal,);
	
	offset = plugin_call(key->plugin->key_ops,
			     get_offset, maxkey);
    
	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE

static errno_t tail40_utmost_key(item_entity_t *item,
				 key_entity_t *key) 
{
	uint64_t offset;

	aal_assert("vpf-442", item != NULL);
	aal_assert("vpf-443", key != NULL);

	key->plugin = item->key.plugin;
	
	if (plugin_call(key->plugin->key_ops, assign, key, &item->key))
		return -EINVAL;

	offset = plugin_call(key->plugin->key_ops,
			     get_offset, key);
	
	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset + item->len - 1);
	
	return 0;
}

#endif

static lookup_t tail40_lookup(item_entity_t *item,
			      key_entity_t *key, 
			      uint32_t *pos)
{
	uint32_t wanted;
	uint32_t current;
    
	key_entity_t curkey;
	key_entity_t maxkey;
    
	aal_assert("umka-1228", item != NULL);
	aal_assert("umka-1229", key != NULL);
	aal_assert("umka-1230", pos != NULL);

	maxkey.plugin = key->plugin;
	
	tail40_maxposs_key(item, &maxkey);

	if (plugin_call(key->plugin->key_ops, compare, key, &maxkey) > 0) {
		*pos = item->len;
		return LP_ABSENT;
	}

	curkey.plugin = key->plugin;
	
	if (plugin_call(curkey.plugin->key_ops, assign, &curkey, &item->key))
		return LP_FAILED;

	current = plugin_call(key->plugin->key_ops, get_offset, &curkey);
    	wanted = plugin_call(key->plugin->key_ops, get_offset, key);
    
	if (wanted >= current && wanted < current + item->len) {
		*pos = wanted - current;
		return LP_PRESENT;
	}

	*pos = item->len;
	return LP_ABSENT;
}

#ifndef ENABLE_STAND_ALONE

static int tail40_mergeable(item_entity_t *item1,
			    item_entity_t *item2)
{
	reiser4_plugin_t *plugin;
	uint64_t offset1, offset2;
	oid_t objectid1, objectid2;
	oid_t locality1, locality2;
	
	aal_assert("umka-1584", item1 != NULL);
	aal_assert("umka-1585", item2 != NULL);

	plugin = item1->key.plugin;
		
	locality1 = plugin_call(plugin->key_ops, get_locality,
				&item1->key);
	
	locality2 = plugin_call(plugin->key_ops, get_locality,
				&item2->key);

	if (locality1 != locality2)
		return 0;
	
	objectid1 = plugin_call(plugin->key_ops, get_objectid,
				&item1->key);
	
	objectid2 = plugin_call(plugin->key_ops, get_objectid,
				&item2->key);

	if (objectid1 != objectid1)
		return 0;

	offset1 = plugin_call(plugin->key_ops, get_offset,
			      &item1->key);
	
	offset2 = plugin_call(plugin->key_ops, get_offset,
			      &item2->key);

	if (offset1 + item1->len != offset2)
		return 0;
	
	return 1;
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t tail40_predict(item_entity_t *src_item,
			      item_entity_t *dst_item,
			      shift_hint_t *hint)
{
	uint32_t space;
	
	aal_assert("umka-1664", src_item != NULL);
	aal_assert("umka-1690", dst_item != NULL);

	space = hint->rest;
		
	if (hint->control & SF_LEFT) {

		if (hint->rest > hint->pos.unit)
			hint->rest = hint->pos.unit;

		hint->pos.unit -= hint->rest;
		
		if (hint->pos.unit == 0 && hint->control & SF_MOVIP) {
			hint->result |= SF_MOVIP;
			hint->pos.unit = dst_item->len + hint->rest;
		}
	} else {
		uint32_t right;

		if (hint->pos.unit < src_item->len) {
			right = src_item->len - hint->pos.unit;
		
			if (hint->rest > right)
				hint->rest = right;

			hint->pos.unit += hint->rest;
			
			if (hint->pos.unit == src_item->len &&
			    hint->control & SF_MOVIP)
			{
				hint->result |= SF_MOVIP;
				hint->pos.unit = 0;
			}
		} else {
			if (hint->control & SF_MOVIP) {
				hint->result |= SF_MOVIP;
				hint->pos.unit = 0;
			}

			hint->rest = 0;
		}
	}

	return 0;
}

static errno_t tail40_shift(item_entity_t *src_item,
			    item_entity_t *dst_item,
			    shift_hint_t *hint)
{
	uint32_t len;
	void *src, *dst;
	
	aal_assert("umka-1665", src_item != NULL);
	aal_assert("umka-1666", dst_item != NULL);
	aal_assert("umka-1667", hint != NULL);

	len = dst_item->len > hint->rest ? dst_item->len - hint->rest :
		dst_item->len;

	if (hint->control & SF_LEFT) {
		
		/* Copying data from the src tail item to dst one */
		aal_memcpy(dst_item->body + len, src_item->body,
			   hint->rest);

		/* Moving src tail data at the start of tail item body */
		src = src_item->body + hint->rest;
		dst = src - hint->rest;
		
		aal_memmove(dst, src, src_item->len - hint->rest);

		/* Updating item's key by the first unit key */
		if (tail40_get_key(src_item, 0, &src_item->key))
			return -EINVAL;
	} else {
		/* Moving dst tail body into right place */
		src = dst_item->body;
		dst = src + hint->rest;
		
		aal_memmove(dst, src, len);

		/* Copying data from src item to dst one */
		aal_memcpy(dst_item->body, src_item->body +
			   src_item->len, hint->rest);

		/* Updating item's key by the first unit key */
		if (tail40_get_key(dst_item, 0, &dst_item->key))
			return -EINVAL;
	}
	
	return 0;
}

static errno_t tail40_feel(item_entity_t *item, uint32_t pos,
			   uint32_t count, copy_hint_t *hint)
{
	aal_assert("umka-1995", item != NULL);
	aal_assert("umka-1996", hint != NULL);

	hint->header_len = 0;
	hint->header_data = NULL;

	hint->body_len = count;
	hint->body_data = item->body + pos;

	return 0;
}

#endif

static reiser4_plugin_t tail40_plugin = {
	.item_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = ITEM_TAIL40_ID,
			.group = TAIL_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "tail40",
#ifndef ENABLE_STAND_ALONE
			.desc = "Tail item for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		
#ifndef ENABLE_STAND_ALONE
		.init	        = tail40_init,
		.write	        = tail40_write,
		.remove	        = tail40_remove,
		.print	        = tail40_print,
		.predict        = tail40_predict,
		.shift	        = tail40_shift,		
		.feel           = tail40_feel,
		.utmost_key     = tail40_utmost_key,
		.gap_key        = tail40_utmost_key,
		.insert	        = NULL,
		.check	        = NULL,
		.estimate       = NULL,
		.set_key        = NULL,
		.layout	        = NULL,
		.layout_check   = NULL,
#endif
		.branch         = NULL,

		.units	        = tail40_units,
		.lookup	        = tail40_lookup,
		.read	        = tail40_read,
		.data		= tail40_data,

#ifndef ENABLE_STAND_ALONE
		.mergeable      = tail40_mergeable,
#else
		.mergeable      = NULL,
#endif
		
		.maxposs_key    = tail40_maxposs_key,
		.get_key        = tail40_get_key
	}
};

static reiser4_plugin_t *tail40_start(reiser4_core_t *c) {
	core = c;
	return &tail40_plugin;
}

plugin_register(tail40_start, NULL);
