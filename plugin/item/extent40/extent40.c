/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40.c -- reiser4 default extent plugin. */

#include "extent40.h"

#define extent40_size(item) \
        (extent40_offset(item, extent40_units(item)))

/* Returns blocksize of the device passed extent @item lies on */
uint32_t extent40_blocksize(item_entity_t *item) {
	aal_assert("umka-2058", item != NULL);
	return item->context.blocksize;
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
uint64_t extent40_offset(item_entity_t *item,
			 uint32_t pos)
{
	extent40_t *extent;
	uint32_t i, blocks = 0;
    
	aal_assert("umka-2204", item != NULL);
	
	extent = extent40_body(item);
	
	for (i = 0; i < pos; i++, extent++)
		blocks += et40_get_width(extent);
    
	return blocks * extent40_blocksize(item);
}

/* Gets the number of unit specified offset lies in */
uint32_t extent40_unit(item_entity_t *item,
		       uint64_t offset)
{
	uint32_t i;
	
#ifndef ENABLE_STAND_ALONE
        uint32_t width = 0;
        extent40_t *extent;
                                                                                         
        extent = extent40_body(item);
                                                                                         
        for (i = 0; i < extent40_units(item);
             i++, extent++)
        {
                                                                                         
                width += et40_get_width(extent) *
                        extent40_blocksize(item);
                                                                                         
                if (offset < width)
                        return i;
        }
#else
	for (i = 0; i < extent40_units(item); i++) {
		if (offset < extent40_offset(item, i + 1))
			return i;
	}
#endif
	
        return i;
}

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
static errno_t extent40_get_key(item_entity_t *item,
				uint32_t pos, 
				key_entity_t *key)
{
	aal_assert("vpf-622", item != NULL);
	aal_assert("vpf-623", key != NULL);

	return body40_get_key(item, pos, key,
			      (trans_func_t)extent40_offset);
}

static int extent40_data(void) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
/* Removes @count byte from passed @item at @pos */
static int32_t extent40_remove(item_entity_t *item,
			       uint32_t pos,
			       uint32_t count)
{
	uint32_t len;
	void *src, *dst;
	
	aal_assert("vpf-941", item != NULL);
	aal_assert("vpf-940", pos < extent40_units(item));

	if (pos + count < extent40_units(item)) {
		dst = extent40_body(item) + pos;
		src = extent40_body(item) + pos + count;

		len = item->len - (pos + count) *
			sizeof(extent40_t);
			
		aal_memmove(dst, src, len);
	}
		
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
		
	if (plugin_call(item->key.plugin->o.key_ops, print,
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
errno_t extent40_maxreal_key(item_entity_t *item,
			     key_entity_t *key) 
{
	aal_assert("vpf-437", item != NULL);
	aal_assert("vpf-438", key  != NULL);

	return body40_maxreal_key(item, key,
				    extent40_offset);
}
#endif

/* Builds maximal possible key for the extent item */
errno_t extent40_maxposs_key(item_entity_t *item,
			     key_entity_t *key) 
{
	aal_assert("umka-1211", item != NULL);
	aal_assert("umka-1212", key != NULL);

	return body40_maxposs_key(item, key);
}

/* Performs lookup for specified @key inside the passed @item. Result of lookup
   will be stored in @pos. */
lookup_t extent40_lookup(item_entity_t *item,
			 key_entity_t *key,
			 uint32_t *pos)
{
	lookup_t res;
	uint64_t offset;

	aal_assert("umka-1500", item != NULL);
	aal_assert("umka-1501", key  != NULL);
	aal_assert("umka-1502", pos != NULL);
	
	/* Using body40_lookup() getting position by key */
	res = body40_lookup(item, key, &offset,
			      (trans_func_t)extent40_offset);

	/* Transforming from the offset ot unit */
	*pos = extent40_unit(item, offset);
	
	return res;
}

/* Reads @count bytes of extent data from the extent item at passed @pos into
   specified @buff. */
static int32_t extent40_read(item_entity_t *item, void *buff,
			     uint32_t pos, uint32_t count)
{
	key_entity_t key;
	uint32_t read, i;
	uint32_t blocksize;
	uint32_t sectorsize;

	aal_assert("umka-1421", item != NULL);
	aal_assert("umka-1422", buff != NULL);
	aal_assert("umka-1672", pos != ~0ul);

	blocksize = extent40_blocksize(item);
	sectorsize = item->context.device->blocksize;

	for (read = count, i = item->pos.unit;
	     i < extent40_units(item) && count > 0; i++)
	{
		uint32_t blkchunk;
		
		/* Here offset is 32 bit value for stand alone mode and
		   apparently code will not be working well with files larger
		   than 4Gb. We can't merely use here uint64_t due to build mode
		   that is without gcc built-in functions like __udivdi3 and
		   __umoddi3 dedicated for working with 64 bit digits. */
		
#ifndef ENABLE_STAND_ALONE
		uint64_t blk, start;
#else
		uint32_t blk, start;
#endif
		extent40_get_key(item, i, &key);

		/* Calculating start block for read */
		start = blk = et40_get_start(extent40_body(item) + i) +
			((pos - (uint32_t)extent40_offset(item, i)) / blocksize);

		/* Loop though the extent blocks */
		while (blk < start + et40_get_width(extent40_body(item) + i) &&
		       count > 0)
		{
			blk_t sec;
			uint32_t blklocal;
			aal_block_t *block;

			blklocal = (pos % blocksize);
			
			if ((blkchunk = blocksize - blklocal) > count)
				blkchunk = count;

			sec = (blk * (blocksize / sectorsize)) +
				(blklocal / sectorsize);

			/* Loop though one block (4096) */
			while (blkchunk > 0) {
				uint32_t secchunk;
				uint32_t seclocal;

				/* Reading one device block (sector) */
				if (!(block = aal_block_read(item->context.device,
							     sectorsize, sec)))
				{
					aal_exception_error("Can't read device "
							    "block %llu.", sec);
					return -EIO;
				}

				/* Calculating data chunk to be copied */
				seclocal = (blklocal % sectorsize);
				
				if ((secchunk = sectorsize - seclocal) > blkchunk)
					secchunk = blkchunk;

				/* Copy data to passed buffer */
				aal_memcpy(buff, block->data + seclocal, secchunk);
				aal_block_free(block);

				if ((seclocal + secchunk) % sectorsize == 0)
					sec++;
					
				pos += secchunk;
				buff += secchunk;
				count -= secchunk;

				blkchunk -= secchunk;
				blklocal += secchunk;
			}

			if (blklocal % blocksize == 0)
				blk++;
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

	return body40_mergeable(item1, item2);
}

static errno_t extent40_estimate_insert(item_entity_t *item,
					create_hint_t *hint,
					uint32_t pos)
{
	aal_assert("umka-1836", hint != NULL);

	hint->len = sizeof(extent40_t);
	return 0;
}

/* Calls @func for each block number extent points to. It is needed for
   calculating fragmentation, etc. */
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
static errno_t extent40_estimate_shift(item_entity_t *src_item,
				       item_entity_t *dst_item,
				       shift_hint_t *hint)
{
	aal_assert("umka-1705", hint != NULL);
	aal_assert("umka-1704", src_item != NULL);

	if (!(src_item->pos.item == hint->pos.item &&
	      hint->pos.unit != ~0ul))
	{
		goto out_update_hint;
	}
	
	if (hint->control & SF_LEFT) {
		uint32_t left;

		/* Check if we need to update insert point at all. If not, we
		   only rely on @hint->rest in that, how many units may be
		   shifted out to neighbour item. */
		if (hint->control & SF_UPTIP) {

			left = hint->pos.unit * sizeof(extent40_t);
			
			if (hint->rest > left)
				hint->rest = left;

			hint->pos.unit -= hint->rest / sizeof(extent40_t);

			if (hint->pos.unit == 0 && hint->control & SF_MOVIP) {
				hint->result |= SF_MOVIP;

				if (dst_item) {
					hint->pos.unit = (dst_item->len + hint->rest) /
						sizeof(extent40_t);
				} else {
					hint->pos.unit = hint->rest / sizeof(extent40_t);
				}
			}
		}
	} else {
		uint32_t right;

		/* The same check as abowe, but for right shift */
		if (hint->control & SF_UPTIP) {

			/* Check is it is possible to move something into right
			   neighbour item. */
			if (hint->pos.unit * sizeof(extent40_t) < src_item->len) {
				right = src_item->len -
					(hint->pos.unit * sizeof(extent40_t));
		
				if (hint->rest > right)
					hint->rest = right;

				if (hint->control & SF_MOVIP &&
				    hint->pos.unit == ((src_item->len - hint->rest) /
						       sizeof(extent40_t)))
				{
					hint->result |= SF_MOVIP;
					hint->pos.unit = 0;
				}
			} else {
				/* There is noning to move, update insert point,
				   flags and out. */
				if (hint->control & SF_MOVIP) {
					hint->result |= SF_MOVIP;
					hint->pos.unit = 0;
				}

				hint->rest = 0;
			}
		}
	}

 out_update_hint:
	hint->units = hint->rest / sizeof(extent40_t);
	return 0;
}

static uint32_t extent40_expand(item_entity_t *item, uint32_t pos,
				uint32_t count, uint32_t len)
{
	/* Preparing space in @dst_item */
	if (pos < extent40_units(item)) {
		uint32_t len;
		void *src, *dst;

		src = (extent40_t *)item->body + pos;
		dst = src + (count * sizeof(extent40_t));

		len = (extent40_units(item) - pos) *
			sizeof(extent40_t);

		aal_memmove(dst, src, len);
	}

	return 0;
}

static uint32_t extent40_shrink(item_entity_t *item, uint32_t pos,
			       uint32_t count, uint32_t len)
{
	/* Srinking @dst_item */
	if (pos < extent40_units(item)) {
		uint32_t len;
		void *src, *dst;

		dst = (extent40_t *)item->body + pos;
		src = dst + (count * sizeof(extent40_t));

		len = (extent40_units(item) - pos) *
			sizeof(extent40_t);

		aal_memmove(dst, src, len);
	}

	return 0;
}

/* Makes copy of units from @src_item to @dst_item */
static errno_t extent40_rep(item_entity_t *dst_item,
			    uint32_t dst_pos,
			    item_entity_t *src_item,
			    uint32_t src_pos,
			    uint32_t count)
{
	/* Copying units from @src_item to @dst_item */
	if (count > 0) {
		void *src, *dst;

		src = (extent40_t *)src_item->body + src_pos;
		dst = (extent40_t *)dst_item->body + dst_pos;
		aal_memmove(dst, src, count * sizeof(extent40_t));
	}
	
	return 0;
}

static errno_t extent40_shift(item_entity_t *src_item,
			      item_entity_t *dst_item,
			      shift_hint_t *hint)
{
	aal_assert("umka-1708", hint != NULL);
	aal_assert("umka-1706", src_item != NULL);
	aal_assert("umka-1707", dst_item != NULL);

	if (hint->control & SF_LEFT) {
		
		/* Preparing space in @dst_item */
		extent40_expand(dst_item, extent40_units(dst_item),
				hint->units, 0);

		/* Copying data from the @src_item to @dst_item */
		extent40_rep(dst_item, extent40_units(dst_item),
			     src_item, 0, hint->units);

		/* Removing units in @src_item */
		extent40_shrink(src_item, 0, hint->units, 0);

		/* Updating item's key by the first unit key */
		extent40_get_key(src_item, hint->units, &src_item->key);
	} else {
		uint32_t pos;
		uint64_t offset;

		/* Preparing space in @dst_item */
		extent40_expand(dst_item, 0, hint->units, 0);

		/* Copying data from the @src_item to @dst_item */
		pos = extent40_units(src_item) - hint->units;
		
		extent40_rep(dst_item, 0, src_item, pos,
			     hint->units);

		/* Removing units in @src_item */
		extent40_shrink(src_item, pos, hint->units, 0);

		/* Updating item's key by the first unit key */
		extent40_get_key(dst_item, 0, &dst_item->key);

		offset = plugin_call(dst_item->key.plugin->o.key_ops,
				     get_offset, &dst_item->key);

		offset -= extent40_offset(dst_item, hint->units);

		plugin_call(dst_item->key.plugin->o.key_ops,
			    set_offset, &dst_item->key, offset);
	}
	
	return 0;
}

extern errno_t extent40_check_struct(item_entity_t *item, uint8_t mode);

extern errno_t extent40_check_layout(item_entity_t *item,
				     region_func_t func, 
				     void *data, uint8_t mode);

extern errno_t extent40_estimate_copy(item_entity_t *dst,
				      uint32_t dst_pos,
				      item_entity_t *src,
				      uint32_t src_pos,
				      copy_hint_t *hint);

extern errno_t extent40_copy(item_entity_t *dst, uint32_t dst_pos, 
			     item_entity_t *src, uint32_t src_pos, 
			     copy_hint_t *hint);
#endif

static reiser4_item_ops_t extent40_ops = {
#ifndef ENABLE_STAND_ALONE
	.copy             = extent40_copy,
	.rep              = extent40_rep,
	.expand           = extent40_expand,
	.shrink           = extent40_shrink,
	.remove	          = extent40_remove,
	.print	          = extent40_print,
	.shift            = extent40_shift,
	.layout           = extent40_layout,
	.check_struct	  = extent40_check_struct,
	.maxreal_key      = extent40_maxreal_key,
	.check_layout     = extent40_check_layout,

	.estimate_copy    = extent40_estimate_copy,
	.estimate_shift   = extent40_estimate_shift,
	.estimate_insert  = extent40_estimate_insert,
	
	.init	          = NULL,
	.insert           = NULL,
	.overhead         = NULL,
	.set_key          = NULL,
	.get_plugid	  = NULL,
#endif
	.branch           = NULL,

	.data	          = extent40_data,
	.read             = extent40_read,
	.units	          = extent40_units,
	.lookup	          = extent40_lookup,

#ifndef ENABLE_STAND_ALONE
	.mergeable        = extent40_mergeable,
#else
	.mergeable        = NULL,
#endif

	.get_key          = extent40_get_key,
	.maxposs_key      = extent40_maxposs_key
};

static reiser4_plugin_t extent40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = ITEM_EXTENT40_ID,
		.group = EXTENT_ITEM,
		.type = ITEM_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "extent40",
		.desc = "Extent item for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.item_ops = &extent40_ops
	}
};

static reiser4_plugin_t *extent40_start(reiser4_core_t *c) {
	return &extent40_plugin;
}

plugin_register(extent40, extent40_start, NULL);
