/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40.c -- reiser4 default extent plugin. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "extent40.h"
#include "extent40_repair.h"

reiser4_core_t *extent40_core;

/* Returns number of units in passed extent @place */
uint32_t extent40_units(reiser4_place_t *place) {
	aal_assert("umka-1446", place != NULL);

#ifdef ENABLE_DEBUG
	if (place->len % sizeof(extent40_t) != 0) {
		aal_error("Node (%llu), item (%u): Invalid extent "
			  "item size (%u) detected.", place_blknr(place),
			  place->pos.item, place->len);
		return 0;
	}
#endif
	
	return place->len / sizeof(extent40_t);
}

/* Calculates extent size. */
uint64_t extent40_offset(reiser4_place_t *place, uint32_t pos) {
	extent40_t *extent;
	uint64_t blocks = 0;
	uint32_t i;
    
	aal_assert("umka-2204", place != NULL);
	
	extent = extent40_body(place);
	
	for (i = 0; i < pos; i++, extent++)
		blocks += et40_get_width(extent);
    
	return blocks * extent40_blksize(place);
}

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
static errno_t extent40_fetch_key(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("vpf-623", key != NULL);
	aal_assert("vpf-622", place != NULL);

	return body40_get_key(place, place->pos.unit,
			      key, extent40_offset);
}

#ifndef ENABLE_MINIMAL
/* Returns item size in bytes */
static uint64_t extent40_size(reiser4_place_t *place) {
	uint32_t units = extent40_units(place);
	return extent40_offset(place, units);
}

/* Returns actual item size on disk */
static uint64_t extent40_bytes(reiser4_place_t *place) {
	extent40_t *extent;
	uint64_t i, blocks;
    
	aal_assert("umka-2204", place != NULL);
	
	extent = extent40_body(place);
	
	/* Count only valuable units. */
	for (blocks = 0, i = 0; i < extent40_units(place);
	     i++, extent++)
	{
		if (et40_get_start(extent))
			blocks += et40_get_width(extent);
	}
    
	return (blocks * extent40_blksize(place));

}

/* Gets the number of unit specified offset lies in. */
uint32_t extent40_unit(reiser4_place_t *place, uint64_t offset) {
	uint32_t i;
        uint32_t width = 0;
        extent40_t *extent;
                                                                                         
        extent = extent40_body(place);
                                                                                         
        for (i = 0; i < extent40_units(place); i++, extent++) {
                width += et40_get_width(extent) *
			extent40_blksize(place);

                if (offset < width)
                        return i;
        }

        return i;
}

/* Removes @count byte from passed @place at @pos */
static errno_t extent40_remove_units(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t len;
	uint32_t pos;
	uint32_t units;
	void *src, *dst;

	aal_assert("vpf-941", place != NULL);
	aal_assert("umka-2402", hint != NULL);

	pos = place->pos.unit;
	units = extent40_units(place);
	
	/* Check for unit pos. */
	if (pos == MAX_UINT32)
		pos = 0;

	aal_assert("umka-3026",
		   pos + hint->count <= units);
	
	/* Calling @hint->region_func for removed region in order to let higher
	   levels know that some extent region is released and perform some
	   actions like release blocks in block allocator, etc. */
	if (hint->region_func) {
		uint32_t i, start;
		extent40_t *extent;

		extent = extent40_body(place) + pos;
			
		for (i = pos; i < pos + hint->count; i++, extent++) {
			errno_t res;
			
			start = et40_get_start(extent);

			if (start == EXTENT_UNALLOC_UNIT || 
			    start == EXTENT_HOLE_UNIT)
				continue;
			
			if ((res = hint->region_func(place, start,
						     et40_get_width(extent), 
						     hint->data)))
				return res;
		}
	}

	/* Removing units from @pos to @hint->count. */
	dst = extent40_body(place) + pos;

	src = extent40_body(place) + pos +
		hint->count;

	len = place->len - (pos + hint->count) *
		sizeof(extent40_t);
			
	aal_memmove(dst, src, len);

	/* Updating item's key by key of first unit. */
	if (pos == 0) {
		if (extent40_fetch_key(place, &place->key))
			return -EINVAL;
	}

	hint->overhead = 0;
	hint->len = sizeof(extent40_t) * hint->count;

	place_mkdirty(place);
	return 0;
}

/* Truncates extent item stating from left by @hint->count bytes. */
static int64_t extent40_trunc_units(reiser4_place_t *place,
				    trans_hint_t *hint)
{
	uint32_t size;
	uint64_t count;
	uint64_t esize;
	uint64_t offset;
	uint32_t blksize;
	
	reiser4_key_t key;
	extent40_t *extent;
	
	aal_assert("umka-2458", place != NULL);
	aal_assert("umka-2461", hint != NULL);

	hint->len = 0;
	hint->bytes = 0;
	hint->overhead = 0;
	
	/* Check for unit pos. */
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	/* Get the amount of bytes from the current unit through the end of 
	   the item. */
	esize = extent40_size(place) - 
		extent40_offset(place, place->pos.unit);

	if ((count = hint->count) > esize)
		count = esize;
	
	extent = extent40_body(place) +
		place->pos.unit;

	blksize = extent40_blksize(place);

	/* Loop though the units until @count of bytes are truncated. We do not
	   increase @place->po.unit, because it will be the same for all ticks
	   of the loop, as units will be removed fully or partially and this
	   means, that @count is over. */
	for (size = count; size > 0; ) {
		uint32_t i;
		uint32_t start;
		uint32_t width;
		uint32_t remove;

		width = et40_get_width(extent);

		/* Calculating what is to be cut out. */
		if ((remove = size / blksize) > width) {
			remove = width;
		} else if (remove == 0) {
			break;
		}
			
		if (extent40_fetch_key(place, &key))
			return -EIO;
	
		/* Removing unit data from the cache */
		for (i = 0; i < remove; i++) {
			aal_hash_table_remove(hint->blocks, &key);

			offset = plug_call(key.plug->o.key_ops,
					   get_offset, &key);

			plug_call(key.plug->o.key_ops, set_offset,
				  &key, (offset + blksize));
		}
		
		start = et40_get_start(extent);
		
		/* Calling region remove notification function. */
		if (start != EXTENT_HOLE_UNIT && start != EXTENT_UNALLOC_UNIT) {
			errno_t res;
			
			if ((res = hint->region_func(place, start, remove, 
						     hint->data)))
				return res;
		}

		hint->bytes += remove * blksize;
		
		/* Check if we remove whole unit. */
		if (remove < width) {
			if (start != EXTENT_HOLE_UNIT &&
			    start != EXTENT_UNALLOC_UNIT)
			{
				et40_inc_start(extent, remove);
			}
			
			et40_dec_width(extent, remove);
			hint->bytes += remove * blksize;
		} else {
			/* Here we remove whole unit. So, we count width blocks
			   to be released, etc. */
			hint->len += sizeof(extent40_t);

			/* Taking care about the rest of extent units if we're
			   bot on the last unit. */
			if (place->pos.unit < extent40_units(place) - 1) {
				
				uint32_t size = sizeof(extent40_t) *
					extent40_units(place) -
					(place->pos.unit + 1);
				
				aal_memmove(extent, extent + 1, size);
			}
		}

		size -= remove * blksize;
	}

	/* Updating key if it makes sense. */
	count = count / blksize * blksize;
	
	if (place->pos.unit == 0 && count) {
		offset = plug_call(place->key.plug->o.key_ops,
				   get_offset, &place->key);
		
		plug_call(place->key.plug->o.key_ops, set_offset,
			  &place->key, offset + count);
	}
	
	return count;
}

/* Builds maximal real key in use for specified @place */
errno_t extent40_maxreal_key(reiser4_place_t *place,
			     reiser4_key_t *key) 
{
	aal_assert("vpf-438", key  != NULL);
	aal_assert("vpf-437", place != NULL);

	return body40_maxreal_key(place, key, extent40_offset);
}
#endif

/* Builds maximal possible key for the extent item */
errno_t extent40_maxposs_key(reiser4_place_t *place,
			     reiser4_key_t *key) 
{
	aal_assert("umka-1211", place != NULL);
	aal_assert("umka-1212", key != NULL);

	return body40_maxposs_key(place, key);
}

/* Performs lookup for specified @key inside the passed @place. Result of lookup
   will be stored in @pos. */
lookup_t extent40_lookup(reiser4_place_t *place,
			 lookup_hint_t *hint,
			 lookup_bias_t bias)
{
	uint64_t offset;
	uint64_t wanted;
	uint32_t i, units;
	extent40_t *extent;

	aal_assert("umka-1500", place != NULL);
	aal_assert("umka-1501", hint != NULL);
	
	extent = extent40_body(place);
	units = extent40_units(place);

	wanted = plug_call(hint->key->plug->o.key_ops,
			   get_offset, hint->key);

	offset = plug_call(hint->key->plug->o.key_ops,
			   get_offset, &place->key);
	
	for (i = 0; i < units; i++, extent++) {
		offset += (et40_get_width(extent) *
			extent40_blksize(place));

		if (offset > wanted) {
			place->pos.unit = i;
			return PRESENT;
		}
	}

	place->pos.unit = units;
	return (bias == FIND_CONV ? PRESENT : ABSENT);
}
#ifndef ENABLE_MINIMAL
/* Reads @count bytes of extent data from the extent item at passed @pos into
   specified @buff. Uses data cache. */
static int64_t extent40_read_units(reiser4_place_t *place,
				   trans_hint_t *hint)
{
	void *buff;
	uint32_t i;
	uint64_t read;
	uint64_t count;
	uint32_t blksize;
	
	reiser4_key_t key;
	aal_block_t *block;

	uint64_t rel_offset;
	uint64_t read_offset;
	uint64_t block_offset;

	aal_assert("umka-1421", place != NULL);
	aal_assert("umka-1422", hint != NULL);

	count = hint->count;
	buff = hint->specific;
	
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	extent40_fetch_key(place, &key);
	blksize = extent40_blksize(place);

	/* Initializing read offset */
	read_offset = plug_call(hint->offset.plug->o.key_ops,
				get_offset, &hint->offset);

	/* Loop through the units until needed amount of data is read or extent
	   item is over. */
	for (read = count, i = place->pos.unit;
	     i < extent40_units(place) && count > 0; i++)
	{
		uint32_t size;
		extent40_t *extent;
		uint64_t blk, start;

		extent = extent40_body(place);

		/* Calculating start block for read. */
		start = et40_get_start(extent + i);
		rel_offset = read_offset - extent40_offset(place, i);
		blk = start + (rel_offset / blksize);

		if (start == EXTENT_HOLE_UNIT) {
			/* Handle the holes. Here we fill @buff by zeros, as
			   hole is detected during read. */
			uint64_t width = et40_get_width(extent + i) - 
				(rel_offset / blksize);
			
			for (; width > 0 && count > 0; count -= size,
				     buff += size, read_offset += size, width--)
			{
				uint32_t rest;

				rest = blksize - (read_offset % blksize);
				
				if ((size = count) > rest)
					size = rest;

				aal_memset(buff, 0, size);
			}
		} else {
			/* Loop though the one unit. */
			while (blk < start + et40_get_width(extent + i) &&
			       count > 0)
			{
				uint32_t rest;

				rest = blksize - (read_offset % blksize);
				
				if ((size = count) > rest)
					size = rest;
				
				/* Initilaizing offset of block needed data lie
				   in. It is needed for getting block from data
				   cache. */
				block_offset = read_offset - (read_offset &
							      (blksize - 1));

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, block_offset);

				/* Getting block from the cache. */
				if (!(block = aal_hash_table_lookup(hint->blocks, &key))) {
					reiser4_key_t *ins_key;
					
					/* If block is not found in cache, we
					   read it and put to cache. */
					aal_device_t *device = extent40_device(place);
				
					if (!(block = aal_block_load(device, blksize,
								     blk)))
					{
						return -EIO;
					}

					if (!(ins_key = aal_calloc(sizeof(*ins_key), 0)))
						return -ENOMEM;

					aal_memcpy(ins_key, &key, sizeof(key));
					
					aal_hash_table_insert(hint->blocks, ins_key, block);
				}

				/* Copying data from found (loaded) block to
				   @buff. */
				aal_memcpy(buff, block->data +
					   (read_offset % blksize), size);

				buff += size;
				count -= size;

				/* Updating read offset and blk next read will
				   be performed from. */
				read_offset += size;

				if ((read_offset % blksize) == 0)
					blk++;
			}
		}
	}
	
	return read;
}
#else
/* Reads @count bytes of extent data from the extent item at passed @pos into
   specified @buff. This function is used in minimal mode. It does not use
   data cache and reads data by 512 bytes chunks. This is needed because of
   GRUB, which has ugly mechanism of getting real block numbers data lie in. */
static int64_t extent40_read_units(reiser4_place_t *place, trans_hint_t *hint) {
	void *buff;
	uint32_t i;
	uint32_t read;
	uint32_t count;
	uint32_t blksize;
	uint32_t secsize;

	reiser4_key_t key;
	uint64_t rel_offset;
	uint64_t read_offset;

	aal_assert("umka-1421", place != NULL);

	buff = hint->specific;
	count = (uint32_t)hint->count;
	
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;
	
	extent40_fetch_key(place, &key);
	
	blksize = extent40_blksize(place);
	secsize = extent40_secsize(place);


	read_offset = plug_call(hint->offset.plug->o.key_ops,
				get_offset, &hint->offset);
	
	rel_offset = read_offset - extent40_offset(place, place->pos.unit);
	
	for (read = count, i = place->pos.unit;
	     i < extent40_units(place) && count > 0; i++, rel_offset = 0)
	{
		uint32_t blkchunk;
		uint64_t blk, start;

		/* Calculating start block for read. */
		start = et40_get_start(extent40_body(place) + i);
		blk = start + aal_div64(rel_offset, blksize, NULL);

		/* Loop though the extent blocks */
		while (blk < start + et40_get_width(extent40_body(place) + i) &&
		       count > 0)
		{
			blk_t sec;
			uint32_t blklocal;

			blklocal = aal_mod64(rel_offset, blksize);
			
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
					
				buff += secchunk;
				count -= secchunk;
				rel_offset += secchunk;

				blkchunk -= secchunk;
				blklocal += secchunk;
			}

			if (blklocal % blksize == 0)
				blk++;
		}
	}
	
	return (uint64_t)read;
}
#endif

/* Updates extent unit at @place by @data */
static int64_t extent40_fetch_units(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t i, pos;
	extent40_t *extent;
	ptr_hint_t *ptr_hint;
	
	aal_assert("umka-2435", hint != NULL);
	aal_assert("umka-2434", place != NULL);
	
	pos = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;

	extent = extent40_body(place) + pos;
	ptr_hint = (ptr_hint_t *)hint->specific;

	for (i = pos; i < pos + hint->count;
	     i++, ptr_hint++, extent++)
	{
		ptr_hint->start = et40_get_start(extent);
		ptr_hint->width = et40_get_width(extent);
	}

	return hint->count;
}

#ifndef ENABLE_MINIMAL
/* Checks if two extent items are mergeable */
static int extent40_mergeable(reiser4_place_t *place1,
			      reiser4_place_t *place2)
{
	aal_assert("umka-2199", place1 != NULL);
	aal_assert("umka-2200", place2 != NULL);
	return body40_mergeable(place1, place2);
}

uint32_t extent40_expand(reiser4_place_t *place,
			 uint32_t pos, uint32_t count)
{
	/* Preparing space in @dst_place */
	if (pos < extent40_units(place)) {
		uint32_t size;
		void *src, *dst;

		src = (extent40_t *)place->body + pos;
		dst = src + (count * sizeof(extent40_t));

		size = (extent40_units(place) - pos - count) *
			sizeof(extent40_t);

		aal_memmove(dst, src, size);
		place_mkdirty(place);
	}

	return 0;
}

uint32_t extent40_shrink(reiser4_place_t *place,
			 uint32_t pos, uint32_t count)
{
	/* Srinking @dst_place. */
	if (pos < extent40_units(place)) {
		uint32_t size;
		void *src, *dst;

		dst = (extent40_t *)place->body + pos;
		src = dst + (count * sizeof(extent40_t));

		size = (extent40_units(place) - pos) *
			sizeof(extent40_t);

		aal_memmove(dst, src, size);
		place_mkdirty(place);
	}

	return 0;
}

/* Makes copy of units from @src_place to @dst_place */
static errno_t extent40_copy(reiser4_place_t *dst_place, uint32_t dst_pos,
			     reiser4_place_t *src_place, uint32_t src_pos,
			     uint32_t count)
{
	/* Copying units from @src_place to @dst_place */
	if (count > 0) {
		uint32_t size;
		void *src, *dst;

		size = count * sizeof(extent40_t);
		src = (extent40_t *)src_place->body + src_pos;
		dst = (extent40_t *)dst_place->body + dst_pos;
		
		aal_memmove(dst, src, size);
		place_mkdirty(dst_place);
	}
	
	return 0;
}

/* Updates extent unit at @place by @data */
static int64_t extent40_update_units(reiser4_place_t *place,
				     trans_hint_t *hint)
{
	uint32_t i, pos;
	extent40_t *extent;
	ptr_hint_t *ptr_hint;
	
	aal_assert("umka-2431", hint != NULL);
	aal_assert("umka-2430", place != NULL);
	
	pos = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;
	
	extent = extent40_body(place) + pos;
	ptr_hint = (ptr_hint_t *)hint->specific;

	for (i = pos; i < pos + hint->count;
	     i++, ptr_hint++, extent++)
	{
		et40_set_start(extent, ptr_hint->start);
		et40_set_width(extent, ptr_hint->width);
	}

	place_mkdirty(place);
	return hint->count;
}

/* Estmates how many bytes is needed to insert @hint->count extent units to
   passed @place. */
static errno_t extent40_prep_insert(reiser4_place_t *place,
				    trans_hint_t *hint)
{
	aal_assert("umka-2426", place != NULL);
	aal_assert("umka-2427", hint != NULL);

	hint->len = hint->count * sizeof(extent40_t);
	return 0;
}

/* Inserts one or more extent units to @place */
static int64_t extent40_insert_units(reiser4_place_t *place,
				     trans_hint_t *hint)
{
	aal_assert("umka-2429", hint != NULL);
	aal_assert("umka-2428", place != NULL);

	/* Expanding extent item at @place */
	extent40_expand(place, place->pos.unit,
			hint->count);
	
	/* Updating @count units at @place */
	return extent40_update_units(place, hint);
}

static int64_t extent40_alloc_block(reiser4_place_t *place,
				    aal_hash_table_t *blocks,
				    uint64_t ins_offset, 
				    uint64_t count) 
{
	reiser4_key_t *ins_key, key;
	aal_device_t *device;
	aal_block_t *block;

	uint64_t offset, bytes;
	uint32_t blksize;
	uint32_t size;

	device = extent40_device(place);
	blksize = extent40_blksize(place);

	/* Get offset aligned to the blksize. */
	offset = (ins_offset - (ins_offset & (blksize - 1)));

	/* Prepare the key of the new allocated block. */
	plug_call(place->key.plug->o.key_ops, assign, &key, &place->key);
	plug_call(key.plug->o.key_ops, set_offset, &key, offset);

	bytes = 0;

	for (; count > 0; count -= size, ins_offset += size) {
		/* Calculating size to be written this time. */
		size = blksize - (ins_offset % blksize);

		if (size > count) 
			size = count;

		if (!(block = aal_block_alloc(device, blksize, 0)))
			return -ENOMEM;

		if (!(ins_key = aal_calloc(sizeof(*ins_key), 0)))
			return -ENOMEM;

		aal_memcpy(ins_key, &key, sizeof(key));

		aal_hash_table_insert(blocks, ins_key, block);

		bytes += blksize;

		/* Update @key offset. */
		offset += blksize;
		plug_call(key.plug->o.key_ops, set_offset, &key, offset);
	}

	return bytes;
}

/* Obtain the block from the block hash table by the @key. */
static aal_block_t *extent40_load_block(reiser4_place_t *place,
					aal_hash_table_t *blocks,
					reiser4_key_t *key,
					blk_t start, blk_t off)
{
	reiser4_key_t *ins_key;
	aal_device_t *device;
	aal_block_t *block;
	uint32_t blksize;
	
	device = extent40_device(place);
	blksize = extent40_blksize(place);

	/* Obtain the block from the block hash table. */
	if ((block = aal_hash_table_lookup(blocks, key)))
		return block;

	if (start == EXTENT_UNALLOC_UNIT) {
		aal_error("Block (%llu), item (%u): "
			  "Unallocated extent unit without attached "
			  "block detected.", place_blknr(place), 
			  place->pos.item);
		return NULL;
	}
	
	/* Loading data block. */
	if (!(block = aal_block_load(device, blksize, start + off))) {
		aal_error("Can't read block %llu. %s.", 
			  start + off, device->error);
		return NULL;
	}

	if (!(ins_key = aal_calloc(sizeof(*ins_key), 0)))
		goto error_free_block;

	aal_memcpy(ins_key, key, sizeof(*key));

	/* Updating block in data cache. */
	aal_hash_table_insert(blocks, ins_key, block);

	return block;
	
 error_free_block:
	aal_block_free(block);
	return NULL;
}

/* Estimates extent write operation */
static errno_t extent40_prep_write(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_key_t key;
	extent40_t *extent;

	uint64_t unit_size;
	uint64_t uni_off;
	uint64_t ins_off;
	uint64_t max_off;
	uint64_t start;
	int64_t count;
	int64_t size;

	uint32_t unit_pos;
	uint32_t blksize;
	uint32_t units;

	aal_assert("umka-1836", hint != NULL);
	aal_assert("umka-2425", place != NULL);

	hint->len = 0;
	hint->overhead = 0;
	unit_pos = place->pos.unit;
	units = place->pos.unit == MAX_UINT32 ? 0 : extent40_units(place);

	/* Set maxkey as the start key of the operation. The amount of bytes 
	   handled will be added later. */
	plug_call(hint->offset.plug->o.key_ops,
		  assign, &hint->maxkey, &hint->offset);

	if (place->pos.unit == MAX_UINT32 || unit_pos == units) {
		/* Inserting a new item or appending units. */

		/* Set maxkey as the limit of the operation. */
		max_off = plug_call(hint->maxkey.plug->o.key_ops,
				    get_offset, &hint->maxkey);

		max_off += hint->count;

		plug_call(hint->maxkey.plug->o.key_ops,
			  set_offset, &hint->maxkey, max_off);

		if (place->pos.unit != MAX_UINT32) {
			uint64_t start;

			/* If new data could be attached to the end of existent,
			   no new unit is needed. It is possible if we write data 
			   over not allocated unit or hole over the hole. */
			extent = extent40_body(place) + units - 1;
			start = et40_get_start(extent);

			if ((start == EXTENT_UNALLOC_UNIT && hint->specific) ||
			    (start == EXTENT_HOLE_UNIT && !hint->specific))
			{
				/* New unit is merged with the last one. */
				hint->insert_flags |= ET40_JOIN;
			} else {
				/* Cannot merge with the last unit. */
				hint->len += sizeof(extent40_t);
			}
		} else {
			/* New item, 1 unit is allocated. */
			hint->len += sizeof(extent40_t);
		}
	} else {
		/* Writing inside item. */

		extent40_fetch_key(place, &key);
		blksize = extent40_blksize(place);

		/* Getting maximal real key, it limits the amount of 
		   data being written at once. */
		uni_off = plug_call(key.plug->o.key_ops,
				    get_offset, &key);

		ins_off = plug_call(hint->offset.plug->o.key_ops,
				    get_offset, &hint->offset);

		aal_assert("vpf-1697", uni_off <= ins_off);

		extent = extent40_body(place) + unit_pos;

		/* This loop checks if we insert some data inside extent and we
		   should take into account possible holes. */
		for (count = hint->count; count > 0 && unit_pos < units;
		     count -= size, unit_pos++, extent++,
		     ins_off += size, uni_off += unit_size)
		{
			/* If some space is already required for the previous 
			   units, handle only it, unit to avoid problems with 
			   splitting units on tree expand. */
			if (hint->len)
				break;

			start = et40_get_start(extent);

			unit_size = et40_get_width(extent) * blksize;
			size = unit_size - (ins_off - uni_off);

			if (size > count)
				size = count;

			/* New unit needs to be allocated if data are written
			   to a hole and not the whole hole is overwritten, in 
			   other words it needs to be splitted, */
			if (start == EXTENT_HOLE_UNIT && hint->specific) {

				/* At least one block is left in the hole. */
				if (ins_off >= uni_off + blksize)
					hint->len += sizeof(extent40_t);

				/* At least one block is left in the hole. */
				if (ins_off + size + blksize <= 
				    uni_off + unit_size)
				{
					hint->len += sizeof(extent40_t);
				}

				/* If we already overwrite some units, do 
				   not require space, just overwrite them 
				   -- tree_expand may split this unit off */
				if (hint->len && unit_pos != place->pos.unit) {
					hint->len = 0;
					break;
				}
			}
		}

		plug_call(hint->maxkey.plug->o.key_ops,
			  set_offset, &hint->maxkey, ins_off);

		hint->insert_flags |= ET40_OVERWRITE;
	}

	return 0;
}

/* Writes data to extent. */
static int64_t extent40_write_units(reiser4_place_t *place, trans_hint_t *hint) {
	aal_device_t *device;
	aal_block_t *block;
	extent40_t *extent;
	reiser4_key_t key;

	uint64_t unit_size;
	uint64_t max_off;
	uint64_t ins_off;
	uint64_t uni_off;
	uint64_t blk_off;
	uint64_t start;
	int64_t count;
	int64_t bytes;
	int64_t size;

	uint32_t unit_pos;
	uint32_t blksize;
	uint32_t width;
	uint32_t units;

	char *buff;


	aal_assert("umka-2357", hint != NULL);
	aal_assert("umka-2356", place != NULL);

	/* Correcting insert point, as it may point to -1 (create new item) and
	   behind last unit (adding data at the end of item). */
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	hint->bytes = 0;

	buff = hint->specific;
	unit_pos = place->pos.unit;
	units = extent40_units(place);
	device = extent40_device(place);
	blksize = extent40_blksize(place);

	/* Get the offset range being handled: ins_off, max_off. */
	ins_off = plug_call(hint->offset.plug->o.key_ops,
			    get_offset, &hint->offset);
	max_off = plug_call(hint->maxkey.plug->o.key_ops,
			    get_offset, &hint->maxkey);

	count = max_off - ins_off;

	if (hint->insert_flags & ET40_OVERWRITE) {
		/* If overwrite requires space, split the unit correctly. */
		extent = extent40_body(place) + unit_pos;

		uni_off = extent40_offset(place, unit_pos);

		/* This loop checks if we insert some data inside extent and we
		   should take into account possible holes. */
		for ( ; count > 0 && unit_pos < units;
		      count -= size, unit_pos++, extent++,
		      ins_off += size, uni_off += unit_size)
		{
			start = et40_get_start(extent);
			width = et40_get_width(extent);

			unit_size = width * blksize;
			size = unit_size - (ins_off - uni_off);

			if (size > count)
				size = count;

			if (start != EXTENT_HOLE_UNIT || !buff) 
				continue;

			/* Overwrite the hole with the data.
			   (1) the whole hole gets overwritten;
			   (2) a part of it, needs to be splitted;
			   (3) a middle part, 2 splits are needed. */
			
			aal_assert("vpf-1700", ins_off >= uni_off);

			if (ins_off >= uni_off + blksize) {
				/* Overwrite the tail. */

				/* Add the unit after the current one. */
				extent40_expand(place, unit_pos + 1, 1);

				/* Get the size of the current. */
				unit_size = ins_off - uni_off;
				unit_size /= blksize;

				aal_assert("vpf-1699", width > unit_size);

				/* Reset widths of the current and the next. */
				et40_set_width(extent, unit_size);
				et40_set_width(extent + 1, width - unit_size);
				et40_set_start(extent + 1, EXTENT_HOLE_UNIT);

				/* Recalc the unit size values. */
				unit_size *= blksize;
				size = 0;
			}

			if (ins_off + size + blksize <= uni_off + unit_size) {
				/* Overwrite the head. */

				/* Add the unit after the current one. */
				extent40_expand(place, unit_pos + 1, 1);

				/* Get the size of the next. */
				unit_size = ins_off + size - uni_off;
				unit_size = (unit_size + blksize - 1) / blksize;

				aal_assert("vpf-1701", width > unit_size);

				/* Reset widths of the current and the next. */
				et40_set_width(extent, unit_size);
				et40_set_width(extent + 1, width - unit_size);
				et40_set_start(extent + 1, EXTENT_HOLE_UNIT);

				/* Recalc the unit size values. */
				unit_size *= blksize;
			}

			if (!size) continue; 
			
			et40_set_start(extent, EXTENT_UNALLOC_UNIT);

			/* Size bytes are to be written to this unit. */
			bytes = extent40_alloc_block(place, hint->blocks,
						     ins_off, size);

			if (bytes < 0) 
				return bytes;

			hint->bytes += bytes;

			aal_assert("vpf-1703", bytes == width * blksize);
		}
	} else {
		int allocate = 0;
		
		/* Calculating width in blocks of new unit to be inserted. */
		width = max_off - ins_off;
		width = (width + blksize - 1) / blksize;

		if (unit_pos && (hint->insert_flags & ET40_JOIN)) {
			/* Some units exist and new get merged with the last. */

			/* Increment the width of the last unit. */
			place->pos.unit--;
			unit_pos--;
			extent = extent40_body(place) + unit_pos;
			start = et40_get_start(extent);

			et40_inc_width(extent, width);
			allocate = (start == EXTENT_UNALLOC_UNIT);
		} else {
			/* First unit or cannot merge with the last one. */
			extent = extent40_body(place);

			/* If some data are being inserted, allocate some 
			   blocks for them later. */
			if (buff) {
				et40_set_start(extent, EXTENT_UNALLOC_UNIT);
				allocate = 1;
			} else {
				et40_set_start(extent, EXTENT_HOLE_UNIT);
			}

			et40_set_width(extent, width);
		}

		if (allocate) {
			if ((bytes = extent40_alloc_block(place, hint->blocks,
							  ins_off, count)) < 0)
			{
				return bytes;
			}

			hint->bytes += bytes;

			aal_assert("vpf-1703", bytes == width * blksize);
		}
	}


	/* Updating @key by unit key as it is changed. */
	extent40_fetch_key(place, &key);

	ins_off = plug_call(hint->offset.plug->o.key_ops,
			    get_offset, &hint->offset);

	extent = extent40_body(place) + place->pos.unit;
	uni_off = plug_call(key.plug->o.key_ops, get_offset, &key);

	/* Second stage -- writing data to allocated blocks. */
	for ( ; count > 0; count -= size, ins_off += size) {
		blk_t blk;
		
		/* Calculating size to be written this time. */
		size = blksize - (ins_off & (blksize - 1));
		/* Block offset we will insert in. */
		blk_off = ins_off - (ins_off & (blksize - 1));

		if (size > count)
			size = count;

		/* Preparing key for getting data by it. */
		plug_call(key.plug->o.key_ops, set_offset, &key, blk_off);

		/* Checking if we write data inside item. */
		aal_assert("vpf-1705", blk_off + size <= max_off);

		start = et40_get_start(extent);
		width = et40_get_width(extent);

		/* Nothing to be done if hole gets overwritten with hole. */
		if (start == EXTENT_HOLE_UNIT && !buff)
			continue;

		/* Blocks must be allocated already. */
		aal_assert("vpf-1706", start != EXTENT_HOLE_UNIT);

		blk = (blk_off - uni_off) / blksize;

		if (!(block = extent40_load_block(place, hint->blocks, 
						  &key, start, blk)))
		{
			return -EINVAL;
		}

		/* Writting data to @block. */
		if (buff) {
			aal_memcpy(block->data + ins_off - blk_off, buff, size);
			buff += size;
		} else {
			/* Writting a hole. */
			aal_memset(block->data + ins_off - blk_off, 0, size);
		}

		block->dirty = 1;

		if (blk == width) {
			extent++;
			uni_off += (width * blksize);
		}
	}
	
	place_mkdirty(place);
	return hint->count - count;
}

/* Calls @region_func for each block number extent points to. It is needed for
   calculating fragmentation, etc. */
static errno_t extent40_layout(reiser4_place_t *place,
			       region_func_t region_func,
			       void *data)
{
	extent40_t *extent;
	uint32_t i, units;
	errno_t res = 0;
	
	aal_assert("umka-1747", place != NULL);
	aal_assert("umka-1748", region_func != NULL);

	extent = extent40_body(place);
	units = extent40_units(place);

	for (i = 0; i < units; i++, extent++) {
		uint64_t start;
		uint64_t width;

		start = et40_get_start(extent);

		if (start == EXTENT_UNALLOC_UNIT || start == EXTENT_HOLE_UNIT)
			continue;
		
		width = et40_get_width(extent);
		
		if ((res = region_func(place, start, width, data)))
			return res;
	}
			
	return 0;
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t extent40_prep_shift(reiser4_place_t *src_place,
				   reiser4_place_t *dst_place,
				   shift_hint_t *hint)
{
	int check_point;
	
	aal_assert("umka-1705", hint != NULL);
	aal_assert("umka-1704", src_place != NULL);

	hint->units_number = 0;

	check_point = (src_place->pos.item == hint->pos.item &&
		       hint->pos.unit != MAX_UINT32);
		
	if (hint->control & SF_ALLOW_LEFT) {
		uint32_t left;

		/* If we have to take into account insert point. */
		if (hint->control & SF_UPDATE_POINT && check_point) {
			left = hint->pos.unit * sizeof(extent40_t);
			
			if (hint->units_bytes > left)
				hint->units_bytes = left;

			hint->pos.unit -= hint->units_bytes /
				sizeof(extent40_t);

			if (hint->pos.unit == 0 &&
			    (hint->control & SF_MOVE_POINT))
			{
				hint->result |= SF_MOVE_POINT;

				if (dst_place) {
					hint->pos.unit = dst_place->len +
						hint->units_bytes;
				
					hint->pos.unit /= sizeof(extent40_t);
				} else {
					hint->pos.unit = hint->units_bytes /
						sizeof(extent40_t);
				}
			}
		} else {
			if (hint->units_bytes > src_place->len)
				hint->units_bytes = src_place->len;
		}
	} else {
		uint32_t right;

		/* The same check as abowe, but for right shift */
		if (hint->control & SF_UPDATE_POINT && check_point) {
			uint32_t units;

			units = extent40_units(src_place);
			
			/* Check is it is possible to move something into right
			   neighbour item. */
			if (hint->pos.unit < units) {
				right = (units - hint->pos.unit) *
					sizeof(extent40_t);
		
				if (hint->units_bytes > right)
					hint->units_bytes = right;

				if ((hint->control & SF_MOVE_POINT) &&
				    hint->units_bytes == right)
				{
					hint->result |= SF_MOVE_POINT;
					hint->pos.unit = 0;
				} else {
					hint->units_bytes = right -
						hint->units_bytes;
				}
			} else {
				/* There is noning to move, update insert point,
				   flags and out. */
				if (hint->control & SF_MOVE_POINT) {
					hint->result |= SF_MOVE_POINT;
					hint->pos.unit = 0;
				}

				hint->units_bytes = 0;
			}
		} else {
			if (hint->units_bytes > src_place->len)
				hint->units_bytes = src_place->len;
		}
	}

	hint->units_bytes -= hint->units_bytes % sizeof(extent40_t);
	hint->units_number = hint->units_bytes / sizeof(extent40_t);
	
	return 0;
}

static errno_t extent40_shift_units(reiser4_place_t *src_place,
				    reiser4_place_t *dst_place,
				    shift_hint_t *hint)
{
	uint32_t pos;
	
	aal_assert("umka-1708", hint != NULL);
	aal_assert("umka-1706", src_place != NULL);
	aal_assert("umka-1707", dst_place != NULL);

	if (hint->control & SF_ALLOW_LEFT) {
		pos = extent40_units(dst_place) - hint->units_number;
			
		/* Preparing space in @dst_place. */
		extent40_expand(dst_place, pos, hint->units_number);

		/* Copying data from the @src_place to @dst_place. */
		extent40_copy(dst_place, pos, src_place, 0,
			      hint->units_number);

		/* Updating item's key by the first unit key. */
		body40_get_key(src_place, hint->units_number,
			       &src_place->key, extent40_offset);

		/* Removing units in @src_place. */
		extent40_shrink(src_place, 0, hint->units_number);
	} else {
		/* Preparing space in @dst_place */
		extent40_expand(dst_place, 0, hint->units_number);

		/* Copying data from the @src_place to @dst_place. */
		pos = extent40_units(src_place) - hint->units_number;
		
		extent40_copy(dst_place, 0, src_place, pos,
			      hint->units_number);
		
		/* Updating item's key by the first unit key. */
		body40_get_key(src_place, pos, &dst_place->key,
			       extent40_offset);

		/* Removing units in @src_place. */
		extent40_shrink(src_place, pos, hint->units_number);
	}
	
	return 0;
}
#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_MINIMAL
	.fuse		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = extent40_mergeable,
	.prep_shift	  = extent40_prep_shift,
	.shift_units	  = extent40_shift_units,
	.maxreal_key	  = extent40_maxreal_key,
	.collision	  = NULL,
#endif
	.units		  = extent40_units,
	.lookup		  = extent40_lookup,
	.fetch_key	  = extent40_fetch_key,
	.maxposs_key      = extent40_maxposs_key
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_MINIMAL
	.remove_units	  = extent40_remove_units,
	.update_units	  = extent40_update_units,
	.prep_insert	  = extent40_prep_insert,
	.insert_units	  = extent40_insert_units,
	.prep_write	  = extent40_prep_write,
	.write_units	  = extent40_write_units,
	.trunc_units	  = extent40_trunc_units,
	.layout		  = extent40_layout,
	.size		  = extent40_size,
	.bytes		  = extent40_bytes,
	.overhead	  = NULL,
#endif
	.read_units	  = extent40_read_units,
	.fetch_units	  = extent40_fetch_units
};

#ifndef ENABLE_MINIMAL
static item_repair_ops_t repair_ops = {
	.check_layout	  = extent40_check_layout,
	.check_struct	  = extent40_check_struct,
	
	.prep_insert_raw  = extent40_prep_insert_raw,
	.insert_raw	  = extent40_insert_raw,

	.pack		  = NULL,
	.unpack		  = NULL
};

static item_debug_ops_t debug_ops = {
	.print		  = extent40_print,
};
#endif

static item_tree_ops_t tree_ops = {
	.down_link	  = NULL,
#ifndef ENABLE_MINIMAL
	.update_link	  = NULL,
#endif
};

static reiser4_item_ops_t extent40_ops = {
	.tree		  = &tree_ops,
	.object		  = &object_ops,
	.balance	  = &balance_ops,
#ifndef ENABLE_MINIMAL
	.repair		  = &repair_ops,
	.debug		  = &debug_ops,
#endif
};

static reiser4_plug_t extent40_plug = {
	.cl    = class_init,
	.id    = {ITEM_EXTENT40_ID, EXTENT_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "extent40",
	.desc  = "Extent item for reiser4. ",
#endif
	.o = {
		.item_ops = &extent40_ops
	}
};

static reiser4_plug_t *extent40_start(reiser4_core_t *c) {
	extent40_core = c;
	return &extent40_plug;
}

plug_register(extent40, extent40_start, NULL);
