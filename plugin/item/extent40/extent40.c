/*
  extent40.c -- reiser4 default extent plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "extent40.h"
#include <aux/aux.h>

static reiser4_core_t *core = NULL;

#define extent40_body(item) ((extent40_t *)item->body)

static uint32_t extent40_blocksize(item_entity_t *item) {
	aal_device_t *device;

	device = item->con.device;
	return aal_device_get_bs(device);
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

static errno_t extent40_unit_key(item_entity_t *item,
				 uint32_t pos, 
				 key_entity_t *key)
{
	uint32_t i;
	extent40_t *extent;
	uint64_t offset, blocksize;
	
	aal_assert("vpf-622", item != NULL, return -1);
	aal_assert("vpf-623", key != NULL, return -1);
	
	aal_assert("vpf-625", pos <  extent40_units(item),
		   return -1);
	
	aal_memcpy(key, &item->key, sizeof(*key));
		
	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, key);

	extent = extent40_body(item);
	blocksize = extent40_blocksize(item);

	for (i = 0; i < pos; i++, extent++)
		offset += et40_get_width(extent) * blocksize;

	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key, offset);
	
	return 0;
}

static errno_t extent40_get_key(item_entity_t *item,
				uint32_t offset, 
				key_entity_t *key) 
{
	aal_assert("vpf-622", item != NULL, return -1);
	aal_assert("vpf-623", key != NULL, return -1);

	aal_assert("vpf-625", offset < extent40_size(item),
		   return -1);
	
	aal_memcpy(key, &item->key, sizeof(*key));
		
	offset += plugin_call(return -1, key->plugin->key_ops,
			     get_offset, key);

	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key, offset);
	
	return 0;
}

#ifndef ENABLE_COMPACT

static errno_t extent40_estimate(item_entity_t *item,
				 reiser4_item_hint_t *hint,
				 uint32_t pos)
{
	return -1;
}

static int32_t extent40_write(item_entity_t *item, void *buff,
			      uint32_t offset, uint32_t count)
{
	uint32_t blocks;
	uint32_t blocksize;
	object_entity_t *alloc;

	aal_assert("umka-1769", item != NULL, return -1);
	aal_assert("umka-1770", buff != NULL, return -1);
	
	blocksize = extent40_blocksize(item);
	blocks = (count / blocksize) + 1;

	alloc = item->con.alloc;

	return count;
}

static errno_t extent40_insert(item_entity_t *item,
			       reiser4_item_hint_t *hint,
			       uint32_t pos)
{
	uint32_t count;
	void *src, *dst;
	uint32_t i, units;

	extent40_t *extent;
	reiser4_ptr_hint_t *ptr_hint;

	aal_assert("umka-1202", item != NULL, return -1); 
	aal_assert("umka-1203", hint != NULL, return -1);
	aal_assert("umka-1656", pos != ~0ul, return -1);

	if (!(extent = extent40_body(item)))
		return -1;

	ptr_hint = (reiser4_ptr_hint_t *)hint;
	
	count = hint->len / sizeof(extent40_t);
	units = extent40_units(item) - count;
	
	/* Preparing room for the new extent units */
	if (pos < units - 1) {
		src = extent + pos;
		dst = extent + pos + count;
		aal_memmove(dst, src, (units - pos) * sizeof(extent40_t));
	}
	
	extent += pos;
	
	for (i = 0; i < count; i++, extent++, ptr_hint++) {
		et40_set_start(extent, ptr_hint->ptr);
		et40_set_width(extent, ptr_hint->width);
	}

	/* Updating item's key by key of the first unit */
	if (pos == 0) {
		if (extent40_get_key(item, 0, &item->key))
			return -1;
	}
	
	return 0;
}

static errno_t extent40_init(item_entity_t *item) {
	aal_assert("umka-1669", item != NULL, return -1);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

static int32_t extent40_remove(item_entity_t *item, uint32_t pos,
			       uint32_t count)
{
	uint32_t units;
	void *src, *dst;
	extent40_t *extent;
	
	aal_assert("umka-1658", item != NULL, return 0);
	aal_assert("umka-1657", pos != ~0ul, return 0);

	if (!(extent = extent40_body(item)))
		return -1;

	units = extent40_units(item);
	aal_assert("umka-1660", pos < units, return -1);

	if (count > units - pos)
		count = units - pos;
	
	if (pos + count < units - 1) {
		dst = extent + pos;
		src = extent + pos + count;

		aal_memmove(dst, src, item->len -
			    ((pos + count) * sizeof(extent40_t)));
	}
		
	if (pos == 0) {
		if (extent40_get_key(item, 0, &item->key))
			return -1;
	}
	
	return (count * sizeof(extent40_t));
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

	aal_stream_format(stream, "EXTENT: len=%u, KEY: ", item->len);
		
	if (plugin_call(return -1, item->key.plugin->key_ops, print,
			&item->key, stream, options))
		return -1;
	
	aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
			  item->plugin->h.id, item->plugin->h.label);
	
	aal_stream_format(stream, "[ ");
	
	for (i = 0; i < count; i++) {
		aal_stream_format(stream, "%llu(%llu)%s",
				  et40_get_start(extent + i),
				  et40_get_width(extent + i),
				  (i < count - 1 ? " " : ""));
	}
	
	aal_stream_format(stream, " ]");
    
	return 0;
}

#endif

static errno_t extent40_max_poss_key(item_entity_t *item,
				     key_entity_t *key) 
{
	uint64_t offset;
	key_entity_t *maxkey;
    
	aal_assert("umka-1211", item != NULL, return -1);
	aal_assert("umka-1212", key != NULL, return -1);

	key->plugin = item->key.plugin;
	
	if (plugin_call(return -1, key->plugin->key_ops,
			assign, key, &item->key))
		return -1;
    
	maxkey = plugin_call(return -1, key->plugin->key_ops,
			     maximal,);
    
	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, maxkey);
    
	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key, offset);

	return 0;
}

static errno_t extent40_max_real_key(item_entity_t *item,
				     key_entity_t *key) 
{
	uint32_t i, blocksize;
	uint64_t delta, offset;
	
	aal_assert("vpf-437", item != NULL, return -1);
	aal_assert("vpf-438", key  != NULL, return -1);

	key->plugin = item->key.plugin;
	
	if (plugin_call(return -1, key->plugin->key_ops,
			assign, key, &item->key))
		return -1;
			
	if ((offset = plugin_call(return -1, key->plugin->key_ops,
				  get_offset, key)))
		return -1;
	
	blocksize = extent40_blocksize(item);

	/* Key offset + for all units { width * blocksize } */
	for (i = 0; i < extent40_units(item); i++) {
		delta = et40_get_width(extent40_body(item) + i);
		
		aal_assert("vpf-439", delta < ((uint64_t) - 1) / 102400, 
			   return -1);

		delta *= blocksize;
		
		aal_assert("vpf-503", offset < ((uint64_t) - 1) - delta, 
			   return -1);
		
		offset += delta;
	}

	plugin_call(return -1, key->plugin->key_ops, set_offset,
		    key, offset - 1);
	
	return 0;	
}

static int extent40_lookup(item_entity_t *item,
			   key_entity_t *key,
			   uint32_t *pos)
{
	uint32_t i, units;
	uint32_t blocksize;
	
	extent40_t *extent;
	key_entity_t maxkey;
	uint64_t offset, lookuped;

	aal_assert("umka-1500", item != NULL, return -1);
	aal_assert("umka-1501", key  != NULL, return -1);
	aal_assert("umka-1502", pos != NULL, return -1);
	
	if (!(extent = extent40_body(item)))
		return -1;
	
	if (extent40_max_poss_key(item, &maxkey))
		return -1;

	if (!(units = extent40_units(item)))
		return -1;
	
	if (plugin_call(return -1, key->plugin->key_ops,
			compare, key, &maxkey) > 0)
	{
		*pos = extent40_size(item);
		return 0;
	}

	lookuped = plugin_call(return -1, key->plugin->key_ops,
			       get_offset, key);

	offset = plugin_call(return -1, key->plugin->key_ops,
			     get_offset, &item->key);

	blocksize = item->con.device->blocksize;
		
	for (i = 0; i < units; i++, extent++) {
		offset += (blocksize * et40_get_width(extent));
		
		if (offset > lookuped) {
			*pos = offset - (offset - lookuped);
			return 1;
		}
	}

	*pos = extent40_size(item);
	return 0;
}

static uint32_t extent40_unit(item_entity_t *item,
			      uint32_t offset)
{
	uint32_t i;
	uint32_t blocksize;
	extent40_t *extent;

	extent = extent40_body(item);
	blocksize = extent40_blocksize(item);
	
	for (i = 0; i < extent40_units(item); i++, extent++) {
		
		if (offset < et40_get_width(extent) * blocksize)
			return i;

		offset -= et40_get_width(extent) * blocksize;
	}

	return i;
}

static int32_t extent40_fetch(item_entity_t *item, void *buff,
			      uint32_t pos, uint32_t count)
{
	uint32_t i, units;
	uint32_t read = 0;
	uint32_t blocksize;

	key_entity_t key;
	extent40_t *extent;
	
	aal_block_t *block;
	aal_device_t *device;

	aal_assert("umka-1421", item != NULL, return -1);
	aal_assert("umka-1422", buff != NULL, return -1);
	aal_assert("umka-1672", pos != ~0ul, return -1);

	extent = extent40_body(item);
	units = extent40_units(item);

	device = item->con.device;
	blocksize = extent40_blocksize(item);

	i = extent40_unit(item, pos);
	
	for (; i < units && read < count; i++) {
		blk_t blk;
		uint64_t start, width;
		uint32_t chunk, offset;

		start = et40_get_start(extent + i);
		width = et40_get_width(extent + i);

		extent40_unit_key(item, i, &key);

		if (!item->key.plugin->key_ops.get_offset)
			return -1;
		
		/* Calculating in-unit local offset */
		offset = plugin_call(return -1, item->key.plugin->key_ops,
				     get_offset, &key);

		offset -= plugin_call(return -1, item->key.plugin->key_ops,
				      get_offset, &item->key);

		start += ((pos - offset) / blocksize);
		
		for (blk = start; blk < start + width && read < count; ) {

			if (!(block = aal_block_open(device, blk))) {
				aal_exception_error("Can't read block %llu. %s.",
						    blk, device->error);
				return -1;
			}

			offset = (pos % blocksize);
			chunk = blocksize - offset;

			if (chunk > count - read)
				chunk = count - read;

			aal_memcpy(buff, block->data + offset, chunk);
					
			aal_block_close(block);
					
			if ((offset + chunk) % blocksize == 0)
				blk++;

			read += chunk; buff += chunk;
		}
	}
	
	return read;
}

#ifndef ENABLE_COMPACT

static errno_t extent40_layout(item_entity_t *item,
			       data_func_t func,
			       void *data)
{
	uint32_t i, units;
	extent40_t *extent;
	
	aal_assert("umka-1747", item != NULL, return -1);
	aal_assert("umka-1748", func != NULL, return -1);

	extent = extent40_body(item);
	units = extent40_units(item);
			
	for (i = 0; i < units; i++, extent++) {
		uint64_t blk;
		uint64_t start;
		uint64_t width;

		start = et40_get_start(extent);
		width = et40_get_start(extent);
				
		for (blk = start; blk < start + width; blk++) {
			errno_t res;
			
			if ((res = func(item, blk, data)))
				return res;
		}
	}
			
	return 0;
}

static int32_t extent40_update(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	uint32_t i;
	extent40_t *extent;
	reiser4_ptr_hint_t *ptr_hint;
	
	aal_assert("umka-1425", item != NULL, return -1);
	aal_assert("umka-1426", buff != NULL, return -1);
	aal_assert("umka-1680", pos != ~0ul, return -1);

	if (!(extent = extent40_body(item)))
		return -1;

	ptr_hint = (reiser4_ptr_hint_t *)buff;

	extent += pos;
	
	for (i = pos; i < extent40_units(item); i++, extent++, ptr_hint++) {
		et40_set_start(extent, ptr_hint->ptr);
		et40_set_width(extent, ptr_hint->width);
	}
	
	if (pos == 0) {
		if (extent40_get_key(item, 0, &item->key))
			return -1;
	}
	
	return i - pos;
}

static int extent40_mergeable(item_entity_t *item1, item_entity_t *item2) {
	reiser4_plugin_t *plugin;
	uint64_t offset1, offset2;
	roid_t objectid1, objectid2;
	roid_t locality1, locality2;
	
	aal_assert("umka-1581", item1 != NULL, return -1);
	aal_assert("umka-1582", item2 != NULL, return -1);

	plugin = item1->key.plugin;
	
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

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t extent40_predict(item_entity_t *src_item,
				item_entity_t *dst_item,
				shift_hint_t *hint)
{
	uint32_t space;
	
	aal_assert("umka-1704", src_item != NULL, return -1);
	aal_assert("umka-1705", dst_item != NULL, return -1);

	space = hint->rest;
		
	if (hint->flags & SF_LEFT) {

		if (hint->rest > hint->pos.unit * sizeof(extent40_t))
			hint->rest = hint->pos.unit * sizeof(extent40_t);

		hint->pos.unit -= hint->rest / sizeof(extent40_t);
		
		if (hint->pos.unit == 0 && hint->flags & SF_MOVIP) {
			hint->pos.unit = (dst_item->len + hint->rest) /
				sizeof(extent40_t);
		}
	} else {
		uint32_t right;

		if (src_item->len > hint->pos.unit * sizeof(extent40_t)) {

			right = src_item->len -
				(hint->pos.unit * sizeof(extent40_t));
		
			if (hint->rest > right)
				hint->rest = right;

			hint->pos.unit += hint->rest / sizeof(extent40_t);
			
			if (hint->flags & SF_MOVIP &&
			    hint->pos.unit == (src_item->len / sizeof(extent40_t)))
			{
				hint->pos.unit = 0;
			}
		} else {
			
			if (hint->flags & SF_MOVIP)
				hint->pos.unit = 0;

			hint->rest = 0;
		}
	}

	return 0;
}

static errno_t extent40_shift(item_entity_t *src_item,
			      item_entity_t *dst_item,
			      shift_hint_t *hint)
{
	uint32_t len;
	void *src, *dst;
	
	aal_assert("umka-1706", src_item != NULL, return -1);
	aal_assert("umka-1707", dst_item != NULL, return -1);
	aal_assert("umka-1708", hint != NULL, return -1);

	len = dst_item->len > hint->rest ? dst_item->len - hint->rest :
		dst_item->len;

	if (hint->flags & SF_LEFT) {
		
		/* Copying data from the src tail item to dst one */
		aal_memcpy(dst_item->body + len, src_item->body,
			   hint->rest);

		/* Moving src tail data at the start of tail item body */
		src = src_item->body + hint->rest;
		dst = src - hint->rest;
		
		aal_memmove(dst, src, src_item->len - hint->rest);

		/* Updating item's key by the first unit key */
		if (extent40_get_key(src_item, 0, &src_item->key))
			return -1;
	} else {
		/* Moving dst tail body into right place */
		src = dst_item->body;
		dst = src + hint->rest;
		
		aal_memmove(dst, src, len);

		/* Copying data from src item to dst one */
		aal_memcpy(dst_item->body, src_item->body +
			   src_item->len, hint->rest);

		/* Updating item's key by the first unit key */
		if (extent40_get_key(dst_item, 0, &dst_item->key))
			return -1;
	}
	
	return 0;
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
		.estimate      = extent40_estimate,
		.remove	       = extent40_remove,
		.print	       = extent40_print,
		.mergeable     = extent40_mergeable,
		.predict       = extent40_predict,
		.shift         = extent40_shift,
		.layout        = extent40_layout,
#else
		.init	       = NULL,
		.update        = NULL,
		.insert	       = NULL,
		.estimate      = NULL,
		.remove	       = NULL,
		.print	       = NULL,
		.mergeable     = NULL, 
		.predict       = NULL,
		.shift         = NULL,
		.layout        = NULL,
#endif
		.belongs       = NULL,
		.check	       = NULL,
		.valid	       = NULL,
		.open          = NULL,
		.set_key       = NULL,

		.lookup	       = extent40_lookup,
		.units	       = extent40_units,
		.fetch         = extent40_fetch,
		.get_key       = extent40_get_key,
		
		.max_poss_key = extent40_max_poss_key,
		.max_real_key = extent40_max_real_key,
		.gap_key      = extent40_max_real_key
	}
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
	core = c;
	return &extent40_plugin;
}

plugin_register(extent40_start, NULL);
