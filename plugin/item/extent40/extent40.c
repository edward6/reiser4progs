/*
  extent40.c -- reiser4 default extent plugin.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "extent40.h"

#define extent40_size(item) \
        (extent40_offset(item, extent40_units(item)))

static reiser4_core_t *core = NULL;

/* Returns blocksize of the device passed extent @item lies on */
static uint32_t extent40_blocksize(item_entity_t *item) {
	aal_assert("umka-2058", item != NULL);
	return item->context.device->blocksize;
}

/* Returns number of units in passed extent @item */
uint32_t extent40_units(item_entity_t *item) {
	aal_assert("umka-1446", item != NULL);

#ifndef ENABLE_STAND_ALONE
	if (item->len % sizeof(extent40_t) != 0) {
		aal_exception_error("Invalid item size detected. "
				    "Node %llu, item %u.",
				    item->context.blk, item->pos);
		return 0;
	}
#endif
	
	return item->len / sizeof(extent40_t);
}

/* Calculates extent size */
static uint64_t extent40_offset(item_entity_t *item,
				uint64_t pos)
{
	uint32_t i, blocks = 0;
    
	aal_assert("umka-2204", item != NULL);
	
	for (i = 0; i < pos; i++) {
		extent40_t *extent = extent40_body(item);
		blocks += et40_get_width(extent + i);
	}
    
	return (blocks * item->context.device->blocksize);
}

/* Gets the number of unit specified offset lies in */
static uint32_t extent40_unit(item_entity_t *item,
			      uint64_t offset)
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
  Builds the key of the unit at @pos and stores it inside passed @key
  variable. It is needed for updating item key after shifting, etc.
*/
static errno_t extent40_get_key(item_entity_t *item,
				uint32_t pos, 
				key_entity_t *key)
{
	aal_assert("vpf-622", item != NULL);
	aal_assert("vpf-623", key != NULL);
	aal_assert("vpf-625", pos < extent40_units(item));
	
	return common40_get_key(item, pos, key, extent40_offset);
}

static int extent40_data(void) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
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

	/* FIXME-UMKA: Not implemented yet */
	
	/* Updating item's key by zero's unit one */
	if (pos == 0) {
		if (extent40_get_key(item, 0, &item->key))
			return -EINVAL;
	}
	
	return 0;
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

	aal_stream_format(stream, "EXTENT PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->h.label, item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=%u\n", count);
	
	aal_stream_format(stream, "[ ");
	
	for (i = 0; i < count; i++) {
		aal_stream_format(stream, "%llu(%llu)%s",
				  et40_get_start(extent + i),
				  et40_get_width(extent + i),
				  (i < count - 1 ? " " : ""));
	}
	
	aal_stream_format(stream, " ]\n");
    
	return 0;
}

/* Builds maximal real key in use for specified @item */
static errno_t extent40_maxreal_key(item_entity_t *item,
				    key_entity_t *key) 
{
	aal_assert("vpf-437", item != NULL);
	aal_assert("vpf-438", key  != NULL);

	return common40_maxreal_key(item, key, extent40_offset);
}
#endif

/* Builds maximal possible key for the extent item */
static errno_t extent40_maxposs_key(item_entity_t *item,
				    key_entity_t *key) 
{
	aal_assert("umka-1211", item != NULL);
	aal_assert("umka-1212", key != NULL);

	return common40_maxposs_key(item, key);
}

/*
  Performs lookup for specified @key inside the passed @item. Result of lookup
  will be stored in @pos.
*/
static lookup_t extent40_lookup(item_entity_t *item,
				key_entity_t *key,
				uint32_t *pos)
{
	lookup_t res;
	uint64_t offset;

	aal_assert("umka-1500", item != NULL);
	aal_assert("umka-1501", key  != NULL);
	aal_assert("umka-1502", pos != NULL);
	
	/* Looking up */
	res = common40_lookup(item, key, &offset,
			      extent40_offset);

	/* Transforming from the offset ot unit */
	*pos = extent40_unit(item, offset);
	
	return res;
}

/*
  Reads @count bytes of extent data from the extent item at passed @pos into
  specified @buff.
*/
static int32_t extent40_read(item_entity_t *item, void *buff,
			     uint32_t pos, uint32_t count)
{
	uint32_t i;

	key_entity_t key;
	extent40_t *extent;
	uint32_t blocksize;
	
	aal_block_t *block;
	uint32_t read = count;

	aal_assert("umka-1421", item != NULL);
	aal_assert("umka-1422", buff != NULL);
	aal_assert("umka-1672", pos != ~0ul);

	extent = extent40_body(item);
	blocksize = extent40_blocksize(item);

	for (i = extent40_unit(item, pos);
	     i < extent40_units(item) && count > 0; i++)
	{
		blk_t blk;
		uint64_t start;
		uint32_t chunk;
		
		/*
		  FIXME-UMKA: Here offset is 32 bit value for stand alone mode
		  and apparently code will not be working well with files larger
		  than 4Gb. We can't merely use here uint64_t due to build mode
		  that is without gcc built-in functions like __udivdi3 and
		  __umoddi3 dedicated for working with 64 bit digits.
		*/
		
#ifndef ENABLE_STAND_ALONE
		uint64_t offset;
#else
		uint32_t offset;
#endif

		extent40_get_key(item, i, &key);

		/* Calculating in-unit local offset */
		offset = plugin_call(item->key.plugin->key_ops,
				     get_offset, &key);

		offset -= plugin_call(item->key.plugin->key_ops,
				      get_offset, &item->key);

		start = et40_get_start(extent + i) +
			((pos - offset) / blocksize);
		
		for (blk = start; blk < start + et40_get_width(extent + i) &&
			     count > 0; )
		{
			if (!(block = aal_block_read(item->context.device, blk))) {
				aal_exception_error("Can't read block "
						    "%llu.", blk);
				return -EIO;
			}

			/* Calculating in-block offset and chunk to be read */
			offset = (pos % blocksize);
			chunk = blocksize - offset;

			if (chunk > count)
				chunk = count;

			aal_memcpy(buff, block->data + offset, chunk);
					
			aal_block_free(block);
					
			if ((offset + chunk) % blocksize == 0)
				blk++;

			count -= chunk; buff += chunk;
		}
	}
	
	return read;
}

#ifndef ENABLE_STAND_ALONE
/* Checks if two extent items are mergeable */
static int extent40_mergeable(item_entity_t *item1,
			      item_entity_t *item2)
{
	aal_assert("umka-2199", item1 != NULL);
	aal_assert("umka-2200", item2 != NULL);

	return common40_mergeable(item1, item2);
}

/*
  Estimates how many bytes is needed to inert extent into the tree. As we will
  use some kind of flush, extents will be allocated in ot and here we just say
  that extent hint need sizeof(extent40_t) bytes.
*/
static errno_t extent40_estimate(item_entity_t *item, uint32_t pos,
				 uint32_t count, create_hint_t *hint)
{
	aal_assert("umka-1836", hint != NULL);

	/*
	  FIXME-UMKA: Here also should be handled case when we need add data to
	  existent allocated extent and we will not need spare at all in some
	  cases.
	*/
	hint->len = sizeof(extent40_t);
	return 0;
}

/*
  Tries to write @count bytes of data from @buff at @pos. Returns the number of
  bytes realy written.
*/
static int32_t extent40_write(item_entity_t *item, void *buff,
			      uint32_t pos, uint32_t count)
{
	extent40_t *extent;
	uint32_t blocksize;

	aal_assert("umka-2112", item != NULL);
	aal_assert("umka-2113", buff != NULL);
	
	extent = extent40_body(item);
	blocksize = extent40_blocksize(item);

	/* Creating unallocated extent with one unit */
	et40_set_start(extent, -1);
	et40_set_width(extent, (count + blocksize - 1) / blocksize);

	/* Here we should attach data from @buff to created extent */
	
	return count;
}

/*
  Calls @func for each block number extent points to. It is needed for
  calculating fragmentation, etc.
*/
static errno_t extent40_layout(item_entity_t *item,
			       region_func_t region_func,
			       void *data)
{
	errno_t res;
	uint32_t i, units;
	extent40_t *extent;
	
	aal_assert("umka-1747", item != NULL);
	aal_assert("umka-1748", region_func != NULL);

	extent = extent40_body(item);
	units = extent40_units(item);

	for (i = 0; i < units; i++, extent++) {
		uint64_t start;
		uint64_t width;

		if (!(start = et40_get_start(extent)))
			continue;
		
		width = et40_get_width(extent);
		
		if ((res = region_func(item, start, width, data)))
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

static errno_t extent40_feel(item_entity_t *item,
			     uint32_t pos,
			     key_entity_t *start,
			     key_entity_t *end,
			     copy_hint_t *hint)
{
	aal_assert("umka-1997", item != NULL);
	aal_assert("umka-1998", hint != NULL);

	/* Not implemented yet */
	return -EINVAL;
}

static errno_t extent40_copy(item_entity_t *dst_item,
			     uint32_t dst_pos,
			     item_entity_t *src_item,
			     uint32_t src_pos,
			     key_entity_t *start,
			     key_entity_t *end,
			     copy_hint_t *hint)
{
	aal_assert("umka-2071", dst_item != NULL);
	aal_assert("umka-2072", src_item != NULL);

	/* Not implemented yet */
	return -EINVAL;
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
		if (extent40_get_key(dst_item, 0, &dst_item->key))
			return -EINVAL;
	}
	
	return 0;
}

extern errno_t extent40_layout_check(item_entity_t *item,
				     region_func_t func, 
				     void *data, uint8_t mode);

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
#ifndef ENABLE_STAND_ALONE
			.desc = "Extent item for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		
#ifndef ENABLE_STAND_ALONE
		.init	       = extent40_init,
		.write         = extent40_write,
		.copy          = extent40_copy,
		.estimate      = extent40_estimate,
		.remove	       = extent40_remove,
		.print	       = extent40_print,
		.predict       = extent40_predict,
		.shift         = extent40_shift,
		.layout        = extent40_layout,
		.check	       = extent40_check,
		.feel          = extent40_feel,
		.gap_key       = extent40_maxreal_key,
		.maxreal_key   = extent40_maxreal_key,
		.layout_check  = extent40_layout_check,
		
		.insert        = NULL,
		.set_key       = NULL,
#endif
		.branch        = NULL,

		.data	       = extent40_data,
		.lookup	       = extent40_lookup,
		.units	       = extent40_units,
		.read          = extent40_read,

#ifndef ENABLE_STAND_ALONE
		.mergeable     = extent40_mergeable,
#else
		.mergeable     = NULL,
#endif

		.get_key       = extent40_get_key,
		.maxposs_key   = extent40_maxposs_key
	}
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
	core = c;
	return &extent40_plugin;
}

plugin_register(extent40, extent40_start, NULL);
