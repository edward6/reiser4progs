/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40.c -- reiser4 default extent plugin. */

#include "extent40.h"

static reiser4_core_t *core = NULL;

/* Returns number of units in passed extent @place */
uint32_t extent40_units(place_t *place) {
	aal_assert("umka-1446", place != NULL);

#ifndef ENABLE_STAND_ALONE
	if (place->len % sizeof(extent40_t) != 0) {
		aal_exception_error("Invalid item size detected. Node "
				    "%llu, item %u.", place->block->nr,
				    place->pos.item);
		return 0;
	}
#endif
	
	return place->len / sizeof(extent40_t);
}

/* Calculates extent size */
uint64_t extent40_offset(place_t *place,
			 uint32_t pos)
{
	extent40_t *extent;
	uint32_t i, blocks = 0;
    
	aal_assert("umka-2204", place != NULL);
	
	extent = extent40_body(place);
	
	for (i = 0; i < pos; i++, extent++)
		blocks += et40_get_width(extent);
    
	return blocks * extent40_blksize(place);
}

/* Gets the number of unit specified offset lies in */
uint32_t extent40_unit(place_t *place,
		       uint64_t offset)
{
	uint32_t i;
	
#ifndef ENABLE_STAND_ALONE
        uint32_t width = 0;
        extent40_t *extent;
                                                                                         
        extent = extent40_body(place);
                                                                                         
        for (i = 0; i < extent40_units(place);
             i++, extent++)
        {
                                                                                         
                width += et40_get_width(extent) *
                        extent40_blksize(place);
                                                                                         
                if (offset < width)
                        return i;
        }
#else
	for (i = 0; i < extent40_units(place); i++) {
		if (offset < extent40_offset(place, i + 1))
			return i;
	}
#endif
	
        return i;
}

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
static errno_t extent40_get_key(place_t *place, uint32_t pos, 
				key_entity_t *key)
{
	aal_assert("vpf-622", place != NULL);
	aal_assert("vpf-623", key != NULL);

	return body40_get_key(place, pos, key,
			      extent40_offset);
}

#ifndef ENABLE_STAND_ALONE
/* Removes @count byte from passed @place at @pos */
static errno_t extent40_remove(place_t *place, uint32_t pos,
			       remove_hint_t *hint)
{
	uint32_t len;
	void *src, *dst;
	
	aal_assert("vpf-941", place != NULL);
	aal_assert("umka-2402", hint != NULL);
	aal_assert("vpf-940", pos < extent40_units(place));

	if (pos + hint->count < extent40_units(place)) {
		dst = extent40_body(place) + pos;

		src = extent40_body(place) + pos +
			hint->count;

		len = place->len - (pos + hint->count) *
			sizeof(extent40_t);
			
		aal_memmove(dst, src, len);
	}
		
	/* Updating item's key by zero's unit one */
	if (pos == 0) {
		if (extent40_get_key(place, 0, &place->key))
			return -EINVAL;
	}

	hint->ohd = 0;
	hint->len = sizeof(extent40_t);

	place_mkdirty(place);
	return 0;
}

/* Prints extent item into specified @stream */
static errno_t extent40_print(place_t *place,
			      aal_stream_t *stream,
			      uint16_t options) 
{
	uint32_t i, count;
	extent40_t *extent;
    
	aal_assert("umka-1205", place != NULL);
	aal_assert("umka-1206", stream != NULL);

	extent = extent40_body(place);
	count = extent40_units(place);

	aal_stream_format(stream, "EXTENT PLUGIN=%s LEN=%u, KEY=[%s] "
			  "UNITS=%u\n[", place->plug->label, place->len,
			  core->key_ops.print(&place->key, PO_DEF), count);
		
	for (i = 0; i < count; i++) {
		aal_stream_format(stream, "%llu(%llu)%s",
				  et40_get_start(extent + i),
				  et40_get_width(extent + i),
				  (i < count - 1 ? " " : ""));
	}
	
	aal_stream_format(stream, "]\n");
    
	return 0;
}

/* Builds maximal real key in use for specified @place */
errno_t extent40_maxreal_key(place_t *place,
			     key_entity_t *key) 
{
	aal_assert("vpf-437", place != NULL);
	aal_assert("vpf-438", key  != NULL);

	return body40_maxreal_key(place, key, extent40_offset);
}
#endif

/* Builds maximal possible key for the extent item */
errno_t extent40_maxposs_key(place_t *place,
			     key_entity_t *key) 
{
	aal_assert("umka-1211", place != NULL);
	aal_assert("umka-1212", key != NULL);

	return body40_maxposs_key(place, key);
}

/* Performs lookup for specified @key inside the passed @place. Result of lookup
   will be stored in @pos. */
lookup_t extent40_lookup(place_t *place,
			 key_entity_t *key,
			 uint32_t *pos)
{
	lookup_t res;
	uint64_t offset;

	aal_assert("umka-1500", place != NULL);
	aal_assert("umka-1501", key  != NULL);
	aal_assert("umka-1502", pos != NULL);
	
	/* Using body40_lookup() getting position by key */
	res = body40_lookup(place, key, &offset,
			    (trans_func_t)extent40_offset);

	/* Transforming from the offset ot unit */
	*pos = extent40_unit(place, offset);
	return res;
}

/* Reads @count bytes of extent data from the extent item at passed @pos into
   specified @buff. */
static int32_t extent40_read(place_t *place, void *buff,
			     uint32_t pos, uint32_t count)
{
	uint32_t read, i;
	uint32_t blksize;
	uint32_t secsize;

	aal_assert("umka-1421", place != NULL);
	aal_assert("umka-1422", buff != NULL);
	aal_assert("umka-1672", pos != MAX_UINT32);

	blksize = extent40_blksize(place);
	secsize = extent40_secsize(place);

	for (read = count, i = place->pos.unit;
	     i < extent40_units(place) && count > 0; i++)
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
		/* Calculating start block for read */
		start = blk = et40_get_start(extent40_body(place) + i) +
			((pos - (uint32_t)extent40_offset(place, i)) /
			 blksize);

		/* Loop though the extent blocks */
		while (blk < start + et40_get_width(extent40_body(place) + i) &&
		       count > 0)
		{
			blk_t sec;
			uint32_t blklocal;

			blklocal = (pos % blksize);
			
			if ((blkchunk = blksize - blklocal) > count)
				blkchunk = count;

			sec = (blk * (blksize / secsize)) +
				(blklocal / secsize);

			/* Loop though one block (4096) */
			while (blkchunk > 0) {
				uint32_t secchunk;
				uint32_t seclocal;
				aal_block_t *block;

				/* Calculating data chunk to be copied */
				seclocal = (blklocal % secsize);
				
				if ((secchunk = secsize - seclocal) > blkchunk)
					secchunk = blkchunk;

				/* Reading one sector */
				if (!(block = aal_block_load(extent40_device(place),
							     secsize, sec)))
				{
					return -EIO;
				}

				/* Copy data to passed buffer */
				aal_memcpy(buff, block->data + seclocal,
					   secchunk);
				
				aal_block_free(block);

				if ((seclocal + secchunk) % secsize == 0)
					sec++;
					
				pos += secchunk;
				buff += secchunk;
				count -= secchunk;

				blkchunk -= secchunk;
				blklocal += secchunk;
			}

			if (blklocal % blksize == 0)
				blk++;
		}
	}
	
	return read;
}

#ifndef ENABLE_STAND_ALONE
/* Checks if two extent items are mergeable */
static int extent40_mergeable(place_t *place1, place_t *place2) {
	aal_assert("umka-2199", place1 != NULL);
	aal_assert("umka-2200", place2 != NULL);

	return body40_mergeable(place1, place2);
}

static errno_t extent40_estimate_insert(place_t *place, uint32_t pos,
					insert_hint_t *hint)
{
	aal_assert("umka-1836", hint != NULL);

	if (pos == MAX_UINT32)
		hint->len = sizeof(extent40_t);
	
/*	else {
		key_entity_t key;
		key_entity_t maxkey;
		
		uint64_t ins_offset;
		uint64_t max_offset;

		extent40_get_key(place, pos, &key);
		extent40_maxreal_key(place, &maxkey);

		ins_offset = plug_call(key.plug->o.key_ops,
				       get_offset, &key);

		ins_offset += hint->offset;

		max_offset = plug_call(maxkey.plug->o.key_ops,
				       get_offset, &maxkey);

		if (ins_offset + hint->count > max_offset)
			hint->len = sizeof(extent40_t);
	} */

	return 0;
}

static errno_t extent40_insert(place_t *place, uint32_t pos,
			       insert_hint_t *hint)
{
	uint32_t blksize;
	key_entity_t key;
	aal_block_t *block;
	extent40_t *extent;

	uint32_t count, size;
	uint64_t unit_offset;
	uint64_t block_offset;
	
	aal_assert("umka-2357", hint != NULL);
	aal_assert("umka-2356", place != NULL);

	blksize = extent40_blksize(place);
	extent = extent40_body(place) + pos;
	
	/* Forming extent unit */
	if (hint->len != 0) {
		et40_set_start(extent, 1);
		et40_set_width(extent, 0);
	}
	
	/* Getting offset of block data will be written in. */
	extent40_get_key(place, pos, &key);

	unit_offset = plug_call(key.plug->o.key_ops,
				get_offset, &key);

	block_offset = (unit_offset + hint->offset) -
		((unit_offset + hint->offset) & (blksize - 1));

	for (hint->bytes = 0, count = hint->count; count > 0;
	     count -= size)
	{
		/* Preparing key for getting data by it */
		plug_call(key.plug->o.key_ops, set_offset,
			  &key, block_offset);

		/* Setting up data block */
		if (!(block = core->tree_ops.get_data(hint->tree, &key))) {
			if (!(block = aal_block_alloc(extent40_device(place),
						      blksize, 0)))
			{
				return -ENOMEM;
			}

			core->tree_ops.set_data(hint->tree, &key, block);
			et40_inc_width(extent, 1);
			hint->bytes += blksize;
		}

		/* Writting data to @block */
		size = blksize;

		if ((hint->offset % blksize) != 0)
			size -= (hint->offset % blksize);
		
		if (size > count)
			size = count;

		if (hint->specific) {
			aal_memcpy(block->data + (hint->offset % blksize),
				   hint->specific, size);
		}

		block_offset += blksize;
	}
	
	place_mkdirty(place);
	return 0;
}

/* Calls @region_func for each block number extent points to. It is needed for
   calculating fragmentation, etc. */
static errno_t extent40_layout(place_t *place,
			       region_func_t region_func,
			       void *data)
{
	errno_t res;
	uint32_t i, units;
	extent40_t *extent;
	
	aal_assert("umka-1747", place != NULL);
	aal_assert("umka-1748", region_func != NULL);

	extent = extent40_body(place);
	units = extent40_units(place);

	for (i = 0; i < units; i++, extent++) {
		uint64_t start;
		uint64_t width;

		if (!(start = et40_get_start(extent)))
			continue;
		
		width = et40_get_width(extent);
		
		if ((res = region_func(place, start, width, data)))
			return res;
	}
			
	return 0;
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t extent40_estimate_shift(place_t *src_place,
				       place_t *dst_place,
				       shift_hint_t *hint)
{
	aal_assert("umka-1705", hint != NULL);
	aal_assert("umka-1704", src_place != NULL);

	if (!(src_place->pos.item == hint->pos.item &&
	      hint->pos.unit != MAX_UINT32))
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

				if (dst_place) {
					hint->pos.unit = (dst_place->len + hint->rest) /
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
			if (hint->pos.unit * sizeof(extent40_t) < src_place->len) {
				right = src_place->len -
					(hint->pos.unit * sizeof(extent40_t));
		
				if (hint->rest > right)
					hint->rest = right;

				if (hint->control & SF_MOVIP &&
				    hint->pos.unit == ((src_place->len - hint->rest) /
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

static uint32_t extent40_expand(place_t *place, uint32_t pos,
				uint32_t count, uint32_t len)
{
	/* Preparing space in @dst_place */
	if (pos < extent40_units(place)) {
		uint32_t len;
		void *src, *dst;

		src = (extent40_t *)place->body + pos;
		dst = src + (count * sizeof(extent40_t));

		len = (extent40_units(place) - pos) *
			sizeof(extent40_t);

		aal_memmove(dst, src, len);
		place_mkdirty(place);
	}

	return 0;
}

static uint32_t extent40_shrink(place_t *place, uint32_t pos,
			       uint32_t count, uint32_t len)
{
	/* Srinking @dst_place */
	if (pos < extent40_units(place)) {
		uint32_t len;
		void *src, *dst;

		dst = (extent40_t *)place->body + pos;
		src = dst + (count * sizeof(extent40_t));

		len = (extent40_units(place) - pos) *
			sizeof(extent40_t);

		aal_memmove(dst, src, len);
		place_mkdirty(place);
	}

	return 0;
}

/* Makes copy of units from @src_place to @dst_place */
static errno_t extent40_rep(place_t *dst_place, uint32_t dst_pos,
			    place_t *src_place, uint32_t src_pos,
			    uint32_t count)
{
	/* Copying units from @src_place to @dst_place */
	if (count > 0) {
		void *src, *dst;

		src = (extent40_t *)src_place->body + src_pos;
		dst = (extent40_t *)dst_place->body + dst_pos;
		aal_memmove(dst, src, count * sizeof(extent40_t));
		
		place_mkdirty(dst_place);
	}
	
	return 0;
}

static errno_t extent40_shift(place_t *src_place, place_t *dst_place,
			      shift_hint_t *hint)
{
	aal_assert("umka-1708", hint != NULL);
	aal_assert("umka-1706", src_place != NULL);
	aal_assert("umka-1707", dst_place != NULL);

	if (hint->control & SF_LEFT) {
		
		/* Preparing space in @dst_place */
		extent40_expand(dst_place, extent40_units(dst_place),
				hint->units, 0);

		/* Copying data from the @src_place to @dst_place */
		extent40_rep(dst_place, extent40_units(dst_place),
			     src_place, 0, hint->units);

		/* Removing units in @src_place */
		extent40_shrink(src_place, 0, hint->units, 0);

		/* Updating item's key by the first unit key */
		extent40_get_key(src_place, hint->units, &src_place->key);
	} else {
		uint32_t pos;
		uint64_t offset;

		/* Preparing space in @dst_place */
		extent40_expand(dst_place, 0, hint->units, 0);

		/* Copying data from the @src_place to @dst_place */
		pos = extent40_units(src_place) - hint->units;
		
		extent40_rep(dst_place, 0, src_place, pos,
			     hint->units);

		/* Removing units in @src_place */
		extent40_shrink(src_place, pos, hint->units, 0);

		/* Updating item's key by the first unit key */
		extent40_get_key(dst_place, 0, &dst_place->key);

		offset = plug_call(dst_place->key.plug->o.key_ops,
				   get_offset, &dst_place->key);

		offset -= extent40_offset(dst_place, hint->units);

		plug_call(dst_place->key.plug->o.key_ops,
			  set_offset, &dst_place->key, offset);
	}
	
	return 0;
}

static uint64_t extent40_size(place_t *place) {
	return extent40_offset(place, extent40_units(place));
}

static uint64_t extent40_bytes(place_t *place) {
	extent40_t *extent;
	uint32_t i, blocks = 0;
    
	aal_assert("umka-2204", place != NULL);
	
	extent = extent40_body(place);
	
	/* Count only valuable units. */
	for (i = 0; i < extent40_units(place); i++, extent++) {
		if (et40_get_start(extent))
			blocks += et40_get_width(extent);
	}
    
	return blocks * extent40_blksize(place);

}

extern errno_t extent40_check_struct(place_t *place,
				     uint8_t mode);

extern errno_t extent40_estimate_copy(place_t *dst,
				      uint32_t dst_pos,
				      place_t *src,
				      uint32_t src_pos,
				      copy_hint_t *hint);

extern errno_t extent40_copy(place_t *dst, uint32_t dst_pos, 
			     place_t *src, uint32_t src_pos, 
			     copy_hint_t *hint);

extern errno_t extent40_check_layout(place_t *place,
				     region_func_t region_func, 
				     void *data, uint8_t mode);
#endif

static reiser4_item_ops_t extent40_ops = {
#ifndef ENABLE_STAND_ALONE
	.copy             = extent40_copy,
	.rep              = extent40_rep,
	.expand           = extent40_expand,
	.shrink           = extent40_shrink,
	.remove	          = extent40_remove,
	.insert           = extent40_insert,
	.print	          = extent40_print,
	.shift            = extent40_shift,
	.layout           = extent40_layout,
	.size		  = extent40_size,
	.bytes		  = extent40_bytes,
	.maxreal_key      = extent40_maxreal_key,
	.check_layout     = extent40_check_layout,
	.check_struct	  = extent40_check_struct,
	.estimate_copy    = extent40_estimate_copy,
	.estimate_shift   = extent40_estimate_shift,
	.estimate_insert  = extent40_estimate_insert,
	
	.init	          = NULL,
	.overhead         = NULL,
	.set_key          = NULL,
#endif
	.branch           = NULL,
	.plugid		  = NULL,

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

static reiser4_plug_t extent40_plug = {
	.cl    = CLASS_INIT,
	.id    = {ITEM_EXTENT40_ID, EXTENT_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "extent40",
	.desc  = "Extent item for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &extent40_ops
	}
};

static reiser4_plug_t *extent40_start(reiser4_core_t *c) {
	core = c;
	return &extent40_plug;
}

plug_register(extent40, extent40_start, NULL);
