/*
  extent40.c -- reiser4 default extent plugin.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "extent40.h"

static reiser4_core_t *core = NULL;

/* Returns blocksize of the device passed extent @item lies on */
static uint32_t extent40_blocksize(item_entity_t *item) {
	aal_device_t *device;

	device = item->con.device;
	return aal_device_get_bs(device);
}

/* Returns number of units in passed extent @item */
uint32_t extent40_units(item_entity_t *item) {
	aal_assert("umka-1446", item != NULL);

	if (item->len % sizeof(extent40_t) != 0) {
		aal_exception_error("Invalid item size detected. Node %llu, "
				    "item %u.", item->con.blk, item->pos);
		return 0;
	}
		
	return item->len / sizeof(extent40_t);
}

/* Calculates extent size */
static uint64_t extent40_size(item_entity_t *item) {
	extent40_t *extent;
	uint32_t i, blocks = 0;
    
	aal_assert("umka-1583", item != NULL);

	extent = extent40_body(item);
	
	for (i = 0; i < extent40_units(item); i++)
		blocks += et40_get_width(extent + i);
    
	return (blocks * aal_device_get_bs(item->con.device));
}

/*
  Builds the key of the unit at @pos and stores it inside passed @key
  variable. It is needed for updating item key after shifting, etc.
*/
static errno_t extent40_unit_key(item_entity_t *item,
				 uint32_t pos, 
				 key_entity_t *key)
{
	uint32_t i;
	extent40_t *extent;
	uint64_t offset, blocksize;
	
	aal_assert("vpf-622", item != NULL);
	aal_assert("vpf-623", key != NULL);
	aal_assert("vpf-625", pos <  extent40_units(item));
	
	aal_memcpy(key, &item->key, sizeof(*key));
		
	offset = plugin_call(key->plugin->key_ops, get_offset, key);

	extent = extent40_body(item);
	blocksize = extent40_blocksize(item);

	/* Calulating key offset to be used */
	for (i = 0; i < pos; i++, extent++)
		offset += et40_get_width(extent) * blocksize;

	plugin_call(key->plugin->key_ops, set_offset, key, offset);
	
	return 0;
}

/*
  Builds unit key like the previous function, but the difference is that key
  offset will be set up to the passed offset. So, it can be not at the start of
  an extent unit.
*/
static errno_t extent40_get_key(item_entity_t *item,
				uint32_t offset, 
				key_entity_t *key) 
{
	aal_assert("vpf-714", item != NULL);
	aal_assert("vpf-715", key != NULL);
	aal_assert("vpf-716", offset < extent40_size(item));
	
	aal_memcpy(key, &item->key, sizeof(*key));
		
	offset += plugin_call(key->plugin->key_ops, get_offset, key);
	plugin_call(key->plugin->key_ops, set_offset, key, offset);
	
	return 0;
}

static int extent40_data () {
	return 1;
}

#ifndef ENABLE_ALONE

static errno_t extent40_init(item_entity_t *item) {
	aal_assert("umka-1669", item != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

/* Removes @count byte from passed @item at @pos */
static int32_t extent40_remove(item_entity_t *item,
			       uint32_t pos,
			       uint32_t count)
{

	aal_assert("umka-1834", item != NULL);

	/* FIXME-UMKA: Here will be extent shrinking code */
	
	/* Updating item's key by zero's unit one */
	if (pos == 0) {
		if (extent40_unit_key(item, 0, &item->key))
			return -1;
	}
	
	return -1;
}

/* Prints extent item into specified @stream */
static errno_t extent40_print(item_entity_t *item,
			      aal_stream_t *stream,
			      uint16_t options) 
{
	uint32_t i, count;
	extent40_t *extent;
    
	aal_assert("umka-1205", item != NULL);
	aal_assert("umka-1206", stream != NULL);

	extent = extent40_body(item);
	count = extent40_units(item);

	aal_stream_format(stream, "EXTENT: len=%u, KEY: ", item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print, &item->key,
			stream, options))
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

extern errno_t extent40_layout_check(item_entity_t *item,
				     region_func_t func, 
				     void *data);

#endif

/* Builds maximal possible key for the extent item */
static errno_t extent40_maxposs_key(item_entity_t *item,
				    key_entity_t *key) 
{
	uint64_t offset;
	key_entity_t *maxkey;
    
	aal_assert("umka-1211", item != NULL);
	aal_assert("umka-1212", key != NULL);

	key->plugin = item->key.plugin;
	
	if (plugin_call(key->plugin->key_ops, assign, key, &item->key))
		return -1;
    
	maxkey = plugin_call(key->plugin->key_ops, maximal,);
    
	offset = plugin_call(key->plugin->key_ops, get_offset, maxkey);
    	plugin_call(key->plugin->key_ops, set_offset, key, offset);

	return 0;
}

/* Builds maximal real key in use for specified @item */
static errno_t extent40_utmost_key(item_entity_t *item,
				   key_entity_t *key) 
{
	uint32_t i, blocksize;
	uint64_t delta, offset;
	
	aal_assert("vpf-437", item != NULL);
	aal_assert("vpf-438", key  != NULL);

	key->plugin = item->key.plugin;
	
	if (plugin_call(key->plugin->key_ops, assign, key, &item->key))
		return -1;
			
	if ((offset = plugin_call(key->plugin->key_ops, get_offset, key)))
		return -1;
	
	blocksize = extent40_blocksize(item);

	/* Key offset + for all units { width * blocksize } */
	for (i = 0; i < extent40_units(item); i++) {
		delta = et40_get_width(extent40_body(item) + i);
		aal_assert("vpf-439", delta < ((uint64_t) - 1) / 102400);

		delta *= blocksize;
		aal_assert("vpf-503", offset < ((uint64_t) - 1) - delta);
		
		offset += delta;
	}

	plugin_call(key->plugin->key_ops, set_offset, key, offset - 1);
	
	return 0;	
}

/*
  Performs lookup for specified @key inside the passed @item. Result of lookup
  will be stored in @pos.
*/
static lookup_t extent40_lookup(item_entity_t *item,
				key_entity_t *key,
				uint32_t *pos)
{
	uint32_t i, units;
	uint32_t blocksize;
	
	extent40_t *extent;
	key_entity_t maxkey;
	uint64_t offset, lookuped;

	aal_assert("umka-1500", item != NULL);
	aal_assert("umka-1501", key  != NULL);
	aal_assert("umka-1502", pos != NULL);
	
	if (!(extent = extent40_body(item)))
		return LP_FAILED;
	
	if (extent40_maxposs_key(item, &maxkey))
		return LP_FAILED;

	if (!(units = extent40_units(item)))
		return LP_FAILED;

	if (plugin_call(key->plugin->key_ops, compare, key, &maxkey) > 0) {
		*pos = extent40_size(item);
		return LP_ABSENT;
	}

	lookuped = plugin_call(key->plugin->key_ops, get_offset, key);
	offset = plugin_call(key->plugin->key_ops, get_offset, &item->key);

	blocksize = item->con.device->blocksize;
		
	for (i = 0; i < units; i++, extent++) {
		offset += (blocksize * et40_get_width(extent));
		
		if (offset > lookuped) {
			*pos = offset - (offset - lookuped);
			return LP_PRESENT;
		}
	}

	*pos = extent40_size(item);
	return LP_ABSENT;
}

/* Gets the number of unit specified offset lies in */
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

/*
  Reads @count bytes of extent data from the extent item at passed @pos into
  specified @buff.
*/
static int32_t extent40_read(item_entity_t *item, void *buff,
			     uint32_t pos, uint32_t count)
{
	uint32_t i, units;
	uint32_t read = 0;
	uint32_t blocksize;

	key_entity_t key;
	extent40_t *extent;
	
	aal_block_t *block;
	aal_device_t *device;

	aal_assert("umka-1421", item != NULL);
	aal_assert("umka-1422", buff != NULL);
	aal_assert("umka-1672", pos != ~0ul);

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

		if (extent40_unit_key(item, i, &key))
			return -1;

		if (!item->key.plugin->key_ops.get_offset)
			return -1;
		
		/* Calculating in-unit local offset */
		offset = plugin_call(item->key.plugin->key_ops,
				     get_offset, &key);

		offset -= plugin_call(item->key.plugin->key_ops,
				      get_offset, &item->key);

		start += ((pos - offset) / blocksize);
		
		for (blk = start; blk < start + width && read < count; ) {

			if (!(block = aal_block_open(device, blk))) {
				aal_exception_error("Can't read block %llu.",
						    blk);
				return -1;
			}

			/* Calculating in-block offset and chunk to be read */
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

/* Checks if two extent items are mergeable */
static int extent40_mergeable(item_entity_t *item1, item_entity_t *item2) {
	reiser4_plugin_t *plugin;
	uint64_t offset1, offset2;
	roid_t objectid1, objectid2;
	roid_t locality1, locality2;
	
	aal_assert("umka-1581", item1 != NULL);
	aal_assert("umka-1582", item2 != NULL);

	plugin = item1->key.plugin;
	
	locality1 = plugin_call(plugin->key_ops, get_locality, &item1->key);
	locality2 = plugin_call(plugin->key_ops, get_locality, &item2->key);

	if (locality1 != locality2)
		return 0;
	
	objectid1 = plugin_call(plugin->key_ops, get_objectid, &item1->key);
	objectid2 = plugin_call(plugin->key_ops, get_objectid, &item2->key);

	if (objectid1 != objectid1)
		return 0;

	offset1 = plugin_call(plugin->key_ops, get_offset, &item1->key);
	offset2 = plugin_call(plugin->key_ops, get_offset, &item2->key);

	if (offset1 + extent40_size(item1) != offset2)
		return 0;
	
	return 1;
}

#ifndef ENABLE_ALONE

/* Deallocates all extent units described by passed @list */
static errno_t extent40_deallocate(item_entity_t *item,
				   aal_list_t *list)
{
	aal_list_t *walk;
	object_entity_t *alloc;

	alloc = item->env.alloc;
	list = aal_list_first(list);
	
	for (walk = aal_list_last(list); walk; ) {
		aal_list_t *temp = aal_list_prev(walk);
		reiser4_ptr_hint_t *ptr = (reiser4_ptr_hint_t *)walk->data;
		
		plugin_call(alloc->plugin->alloc_ops, release_region, alloc,
			    ptr->ptr, ptr->width);
		
		walk = temp;
	}
	
	aal_list_free(list);
	return 0;
}

/*
  Allocates extent units needed for storing @blocks of data and put them into
  list. This function is called from extent40_estimate.
*/
static aal_list_t *extent40_allocate(item_entity_t *item,
				     uint32_t blocks)
{
	object_entity_t *alloc;
	aal_list_t *list = NULL;

	alloc = item->env.alloc;
	
	/*
	  Calling block allocator in order to allocate blocks needed for storing
	  @count bytes of data in extent item. This should be done inside the
	  loop, because it is not a guaranty that block allocator will give us
	  requested block count in once.
	*/
	while (blocks > 0) {
		reiser4_ptr_hint_t *ptr;

		if (!(ptr = aal_calloc(sizeof(*ptr), 0)))
			return NULL;
		
		ptr->width = plugin_call(alloc->plugin->alloc_ops,
					 allocate_region, alloc,
					 &ptr->ptr, blocks);

		if (ptr->width == 0) {
			aal_exception_error("There is no free space enough to "
					    "allocate %lu blocks.", blocks);

			if (list)
				extent40_deallocate(item, list);
			
			aal_free(ptr);
			
			return NULL;
		}

		blocks -= ptr->width;
		
		/* Adding found extent into list */
		list = aal_list_append(list, ptr);
	}

	return aal_list_first(list);
}

/*
  Estimates how many bytes may take extent item/unit(s) for storing @count bytes
  of data at @pos. This function allocates also blocks for all data to be stored
  in oder to determine unit count to be added.
*/
static errno_t extent40_estimate(item_entity_t *item, void *buff,
				 uint32_t pos, uint32_t count)
{
	uint64_t size;
	uint32_t blocksize;

	uint32_t blocks = 0;
	aal_list_t *list = NULL;
	reiser4_item_hint_t *hint;

	aal_assert("umka-1836", buff != NULL);
	
	hint = (reiser4_item_hint_t *)buff;
	aal_assert("umka-1838", hint->env.alloc != NULL);
	
	size = extent40_size(item);
	blocksize = item->con.device->blocksize;

	/* Getting block number needed for allocating if any */
	if (pos >= size && pos - size >= blocksize) {
		uint32_t hole = (pos - size);
		
		blocks = (hole + (blocksize - 1)) / blocksize;
		blocks += (count + (blocksize - 1)) / blocksize;
	} else {
		uint32_t rest = (size - pos);

		if (count > rest) {
			rest = count - rest;
			blocks = (rest + (blocksize - 1)) / blocksize;
		}
	}

	/*
	  Allocating all extent units. Probably it may be done on "write", but
	  here we need it because we need know how many units will be write.
	*/
	if (!(list = extent40_allocate(item, blocks)))
		return -1;

	hint->data = (void *)list;
	hint->len = sizeof(extent40_t) * aal_list_length(list);
	
	return 0;
}

/*
  Tries to write @count bytes of data from @buff at @pos. Returns the number of
  bytes realy written.
*/
static int32_t extent40_write(item_entity_t *item, void *buff,
			      uint32_t pos, uint32_t count)
{
	uint64_t size;
	uint32_t unit;
	uint32_t blocksize;

	extent40_t *extent;
	aal_device_t *device;
	reiser4_item_hint_t *hint;
	
	aal_assert("umka-1832", item != NULL);
	aal_assert("umka-1833", buff != NULL);

	hint = (reiser4_item_hint_t *)buff;
	aal_assert("umka-1838", hint->env.alloc != NULL);
	
	device = item->con.device;
	extent = extent40_body(item);
	blocksize = device->blocksize;
	
	size = extent40_size(item);
	unit = extent40_unit(item, pos);

	/* Checking if we should insert holes */
	if (pos >= size && pos - size >= blocksize) {
		uint32_t width;

		width = ((pos - size) + (blocksize - 1)) /
			blocksize;
			
		et40_set_start(extent + unit, 0);
		et40_set_width(extent + unit, width);
	} else {
	}

	extent40_deallocate(item, hint->data);
	
        /* Updating the key */
	if (pos == 0) {
		if (extent40_unit_key(item, 0, &item->key))
			return -1;
	}
	
	return 0;
}

/*
  Calls @func for each block number extent points to. It is needed for
  calculating fragmentation, etc.
*/
static errno_t extent40_layout(item_entity_t *item,
			       region_func_t func,
			       void *data)
{
	errno_t res;
	uint32_t i, units;
	extent40_t *extent;
	
	aal_assert("umka-1747", item != NULL);
	aal_assert("umka-1748", func != NULL);

	extent = extent40_body(item);
	units = extent40_units(item);

	for (i = 0; i < units; i++, extent++) {
		uint64_t start;
		uint64_t width;

		start = et40_get_start(extent);
		width = et40_get_start(extent);
				
		if (start && (res = func(item, start, width, data)))
			return res;
	}
			
	return 0;
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t extent40_predict(item_entity_t *src_item,
				item_entity_t *dst_item,
				shift_hint_t *hint)
{
	uint32_t space;
	
	aal_assert("umka-1704", src_item != NULL);
	aal_assert("umka-1705", dst_item != NULL);

	space = hint->rest;
		
	if (hint->control & SF_LEFT) {

		if (hint->rest > hint->pos.unit * sizeof(extent40_t))
			hint->rest = hint->pos.unit * sizeof(extent40_t);

		hint->pos.unit -= hint->rest / sizeof(extent40_t);
		
		if (hint->pos.unit == 0 && hint->control & SF_MOVIP) {
			hint->result |= SF_MOVIP;
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
			
			if (hint->control & SF_MOVIP &&
			    hint->pos.unit == (src_item->len / sizeof(extent40_t)))
			{
				hint->pos.unit = 0;
				hint->result |= SF_MOVIP;
			}
		} else {
			
			if (hint->control & SF_MOVIP) {
				hint->pos.unit = 0;
				hint->result |= SF_MOVIP;
			}

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
	
	aal_assert("umka-1706", src_item != NULL);
	aal_assert("umka-1707", dst_item != NULL);
	aal_assert("umka-1708", hint != NULL);

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

extern errno_t extent40_check(item_entity_t *item, uint8_t mode);

#endif

static reiser4_plugin_t extent40_plugin = {
	.item_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = ITEM_EXTENT40_ID,
			.group = EXTENT_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "extent40",
			.desc = "Extent item for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE
		.init	       = extent40_init,
		.write         = extent40_write,
		.estimate      = extent40_estimate,
		.remove	       = extent40_remove,
		.print	       = extent40_print,
		.predict       = extent40_predict,
		.shift         = extent40_shift,
		.layout        = extent40_layout,
		.check	       = extent40_check,
		.layout_check  = extent40_layout_check,

		.insert	       = NULL,
		.set_key       = NULL,
#endif
		.belongs       = NULL,
		.valid	       = NULL,
		.branch        = NULL,

		.data	       = extent40_data,
		.lookup	       = extent40_lookup,
		.units	       = extent40_units,
		.read          = extent40_read,
		.mergeable     = extent40_mergeable,

		.gap_key       = extent40_utmost_key,
		.maxposs_key   = extent40_maxposs_key,
		.utmost_key    = extent40_utmost_key,
		.get_key       = extent40_get_key
	}
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
	core = c;
	return &extent40_plugin;
}

plugin_register(extent40_start, NULL);
