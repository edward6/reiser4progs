/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40.c -- reiser4 default extent plugin. */

#include "extent40.h"
#include "extent40_repair.h"

reiser4_core_t *extent40_core;

/* Returns number of units in passed extent @place */
uint32_t extent40_units(place_t *place) {
	aal_assert("umka-1446", place != NULL);

#ifndef ENABLE_STAND_ALONE
	if (place->len % sizeof(extent40_t) != 0) {
		aal_error("Invalid item size detected. Node "
			  "%llu, item %u.", place->block->nr,
			  place->pos.item);
		return 0;
	}
#endif
	
	return place->len / sizeof(extent40_t);
}

/* Calculates extent size. */
uint64_t extent40_offset(place_t *place, uint32_t pos) {
	extent40_t *extent;
	uint32_t i, blocks = 0;
    
	aal_assert("umka-2204", place != NULL);
	
	extent = extent40_body(place);
	
	for (i = 0; i < pos; i++, extent++)
		blocks += et40_get_width(extent);
    
	return blocks * extent40_blksize(place);
}

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
static errno_t extent40_fetch_key(place_t *place, key_entity_t *key) {
	aal_assert("vpf-623", key != NULL);
	aal_assert("vpf-622", place != NULL);

	return body40_get_key(place, place->pos.unit,
			      key, extent40_offset);
}

#ifndef ENABLE_STAND_ALONE
/* Returns item size in bytes */
static uint64_t extent40_size(place_t *place) {
	uint32_t units = extent40_units(place);
	return extent40_offset(place, units);
}

/* Returns actual item size on disk */
static uint64_t extent40_bytes(place_t *place) {
	extent40_t *extent;
	uint32_t i, blocks;
    
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
uint32_t extent40_unit(place_t *place, uint64_t offset) {
	uint32_t i;
        uint32_t width = 0;
        extent40_t *extent;
                                                                                         
        extent = extent40_body(place);
                                                                                         
        for (i = 0; i < extent40_units(place); i++, extent++) {
                width += et40_get_width(extent) * extent40_blksize(place);

                if (offset < width)
                        return i;
        }

        return i;
}

/* Removes @count byte from passed @place at @pos */
static errno_t extent40_remove_units(place_t *place, trans_hint_t *hint) {
	uint32_t len;
	uint32_t pos;
	uint32_t units;
	void *src, *dst;

	aal_assert("vpf-941", place != NULL);
	aal_assert("umka-2402", hint != NULL);

	pos = place->pos.unit;
	units = extent40_units(place);

	aal_assert("umka-3026",
		   pos + hint->count <= units);
	
	/* Calling @hint->region_func for removed region in order to let higher
	   levels know that some extent region is released and perform some
	   actions like release blocks in block allocator, etc. */
	if (hint->region_func) {
		uint32_t i;
		extent40_t *extent;

		extent = extent40_body(place) + pos;
			
		for (i = pos; i < pos + hint->count; i++, extent++) {
			if (!et40_get_start(extent))
				continue;
			
			hint->region_func(place, et40_get_start(extent),
					  et40_get_width(extent), hint->data);
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
static int64_t extent40_trunc_units(place_t *place,
				    trans_hint_t *hint)
{
	uint32_t pos;
	uint32_t size;
	uint64_t count;
	uint64_t esize;
	uint64_t offset;
	uint32_t blksize;
	
	key_entity_t key;
	extent40_t *extent;
	
	aal_assert("umka-2458", place != NULL);
	aal_assert("umka-2461", hint != NULL);

	hint->overhead = 0;
	hint->len = 0;
	hint->bytes = 0;

	/* Check for unit pos */
	pos = place->pos.unit;
	
	if (pos == MAX_UINT32)
		pos = 0;

	esize = extent40_size(place);

	if ((count = hint->count) > esize)
		count = esize;
	
	blksize = extent40_blksize(place);
	extent = extent40_body(place) + pos;

	for (size = count; size > 0; ) {
		uint32_t width;
		uint32_t chunk;
		uint32_t remove;

		width = et40_get_width(extent);

		/* Calculating chunk to be cut out. */
		if ((chunk = size) > (width * blksize))
			chunk = (width * blksize);

		/* Check if we remove whole unit. */
		if ((remove = (chunk / blksize)) < width) {
			uint32_t i;

			/* Initializing key before removing attached data */
			plug_call(hint->offset.plug->o.key_ops,
				  assign, &key, &hint->offset);
		
			/* Removing unit data from the cache */
			for (i = 0; i < remove; i++) {
				extent40_core->tree_ops.rem_data(hint->tree,
								 &key);
			
				offset = plug_call(key.plug->o.key_ops,
						   get_offset, &key);

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, (offset * blksize));
			}

			/* Calling region remove notification function. */
			hint->region_func(place, et40_get_start(extent),
					  remove, hint->data);
				
			/* Making extent unit shorter */
			et40_inc_start(extent, remove);
			et40_dec_width(extent, remove);
			hint->bytes += remove * blksize;
		} else {
			/* Calling region remove notification function. */
			hint->region_func(place, et40_get_start(extent),
					  width, hint->data);
			
			/* Here we remove whole unit. So, we count width blocks
			   to be released, etc. */
			hint->len += sizeof(extent40_t);
			hint->bytes += width * blksize;

			/* Taking care about the rest of extent units if we're
			   on the last unit. */
			if (pos < extent40_units(place) - 1) {
				
				uint32_t size = sizeof(extent40_t) *
					extent40_units(place) - (pos + 1);
				
				aal_memmove(extent, extent + 1, size);
			}
		}

		size -= chunk;
	}

	/* Updating key if it makes sense. */
	if (pos == 0 && place->len > (uint32_t)hint->len) {
		offset = plug_call(place->key.plug->o.key_ops,
				   get_offset, &place->key);
		
		plug_call(place->key.plug->o.key_ops, set_offset,
			  &place->key, offset + count);
	}
	
	return count;
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
lookup_t extent40_lookup(place_t *place, key_entity_t *key,
			 bias_t bias)
{
	uint64_t offset;
	uint64_t wanted;
	uint32_t i, units;
	extent40_t *extent;

	aal_assert("umka-1500", place != NULL);
	aal_assert("umka-1501", key  != NULL);
	
	extent = extent40_body(place);
	units = extent40_units(place);

	wanted = plug_call(key->plug->o.key_ops,
			   get_offset, key);

	offset = plug_call(key->plug->o.key_ops,
			   get_offset, &place->key);
	
	for (i = 0; i < units; i++, extent++) {
		offset += et40_get_width(extent) *
			extent40_blksize(place);

		if (offset > wanted) {
			place->pos.unit = i;
			return PRESENT;
		}
	}

	place->pos.unit = units;
	return (bias == FIND_CONV ? PRESENT : ABSENT);
}

#ifndef ENABLE_STAND_ALONE
/* Reads @count bytes of extent data from the extent item at passed @pos into
   specified @buff. Uses data cache. */
static int64_t extent40_read_units(place_t *place,
				   trans_hint_t *hint)
{
	void *buff;
	uint32_t i;
	uint64_t read;
	uint64_t count;
	uint32_t blksize;
	
	key_entity_t key;
	aal_block_t *block;

	uint64_t rel_offset;
	uint64_t read_offset;
	uint64_t block_offset;

	aal_assert("umka-1421", place != NULL);
	aal_assert("umka-1422", buff != NULL);

	count = hint->count;
	buff = hint->specific;
	
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	extent40_fetch_key(place, &key);
	blksize = extent40_blksize(place);

	/* Initializing read offset */
	read_offset = plug_call(hint->offset.plug->o.key_ops,
				get_offset, &hint->offset);

	rel_offset = read_offset - plug_call(key.plug->o.key_ops,
					     get_offset, &key);

	/* Loop through the units until needed amount of data is read or extent
	   item is over. */
	for (read = count, i = place->pos.unit;
	     i < extent40_units(place) && count > 0; i++)
	{
		uint32_t size;
		uint64_t blk, start;

		/* Size to read */
		if ((size = count) > blksize - (read_offset % blksize))
			size = blksize - (read_offset % blksize);
		
		/* Calculating start block for read */
		start = blk = et40_get_start(extent40_body(place) + i) +
			(rel_offset / blksize);

		/* Handle hole */
		if (start == EXTENT_SPARSE_UNIT) {
			count -= size;
			aal_memset(buff, 0, size);
		} else {
			extent40_t *extent = extent40_body(place);

			/* Loop though the one unit. */
			while (blk < start + et40_get_width(extent + i) &&
			       count > 0)
			{
				/* Initilaizing offset of block needed data lie
				   in. It is needed for getting block from data
				   cache. */
				block_offset = read_offset - (read_offset &
							      (blksize - 1));

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, block_offset);

				/* Getting block from the cache. */
				if (!(block = extent40_core->tree_ops.get_data(hint->tree,
									       &key)))
				{
					/* If block is not found in cache, we
					   read it and put to cache. */
					aal_device_t *device = extent40_device(place);
				
					if (!(block = aal_block_load(device, blksize,
								     blk)))
					{
						return -EIO;
					}
					
					extent40_core->tree_ops.put_data(hint->tree,
									 &key, block);
				}

				/* Copying data from found (loaded) block to
				   @buff */
				aal_memcpy(buff, block->data +
					   (read_offset % blksize), size);

				count -= size;
			}
		}

		/* Updating buffer pointer and read offset, as it is used for
		 * getting correct block offset for next read. */
		buff += size;
		read_offset += size;

		/* Updating block number to be used on next read */
		if ((read_offset % blksize) == 0) {
			blk++;
		}
	}
	
	return read;
}
#else
/* Reads @count bytes of extent data from the extent item at passed @pos into
   specified @buff. This function is used in stand alone mode. It does not use
   data cache and reads data by 512 bytes chunks. This is needed because of
   GRUB, which has ugly mechanism of getting real block numbers data lie in. */
static int64_t extent40_read_units(place_t *place, trans_hint_t *hint) {
	void *buff;
	uint32_t i;
	uint32_t read;
	uint32_t count;
	uint32_t blksize;
	uint32_t secsize;

	key_entity_t key;
	uint64_t rel_offset;
	uint64_t read_offset;

	aal_assert("umka-1421", place != NULL);
	aal_assert("umka-1422", buff != NULL);

	buff = hint->specific;
	count = (uint32_t)hint->count;
	
	extent40_fetch_key(place, &key);
	
	blksize = extent40_blksize(place);
	secsize = extent40_secsize(place);

	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	read_offset = plug_call(hint->offset.plug->o.key_ops,
				get_offset, &hint->offset);
	
	rel_offset = read_offset - plug_call(key.plug->o.key_ops,
					     get_offset, &key);
	
	for (read = count, i = place->pos.unit;
	     i < extent40_units(place) && count > 0; i++)
	{
		uint32_t blkchunk;
		uint64_t blk, start;

		/* Calculating start block for read. */
		start = blk = et40_get_start(extent40_body(place) + i) +
			aal_div64(rel_offset, blksize, NULL);

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
static int64_t extent40_fetch_units(place_t *place, trans_hint_t *hint) {
	uint32_t i, pos;
	extent40_t *extent;
	ptr_hint_t *ptr_hint;
	
	aal_assert("umka-2435", hint != NULL);
	aal_assert("umka-2434", place != NULL);
	
	pos = place->pos.unit;
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

#ifndef ENABLE_STAND_ALONE
/* Checks if two extent items are mergeable */
static int extent40_mergeable(place_t *place1, place_t *place2) {
	aal_assert("umka-2199", place1 != NULL);
	aal_assert("umka-2200", place2 != NULL);
	return body40_mergeable(place1, place2);
}

uint32_t extent40_expand(place_t *place, uint32_t pos, uint32_t count) {
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

uint32_t extent40_shrink(place_t *place, uint32_t pos, uint32_t count) {
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
static errno_t extent40_copy(place_t *dst_place, uint32_t dst_pos,
			     place_t *src_place, uint32_t src_pos,
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
static int64_t extent40_update_units(place_t *place,
				     trans_hint_t *hint)
{
	uint32_t i, pos;
	extent40_t *extent;
	ptr_hint_t *ptr_hint;
	
	aal_assert("umka-2431", hint != NULL);
	aal_assert("umka-2430", place != NULL);
	
	pos = place->pos.unit;
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
static errno_t extent40_prep_insert(place_t *place,
				    trans_hint_t *hint)
{
	aal_assert("umka-2426", place != NULL);
	aal_assert("umka-2427", hint != NULL);

	hint->len = hint->count * sizeof(extent40_t);
	return 0;
}

/* Inserts one or more extent units to @place */
static int64_t extent40_insert_units(place_t *place,
				     trans_hint_t *hint)
{
	aal_assert("umka-2429", hint != NULL);
	aal_assert("umka-2428", place != NULL);

	/* Expanding extent item at @place */
	extent40_expand(place, place->pos.unit, hint->count);

	/* Updating @count units at @place */
	return extent40_update_units(place, hint);
}

/* Estimates extent write operation */
static errno_t extent40_prep_write(place_t *place,
				   trans_hint_t *hint)
{
	aal_assert("umka-1836", hint != NULL);
	aal_assert("umka-2425", place != NULL);

	if (place->pos.unit == MAX_UINT32) {
		/* Assigning maxkey to key of new created item */
		plug_call(hint->offset.plug->o.key_ops,
			  assign, &hint->maxkey, &hint->offset);
		
		/* Insert point is -1, thus, this is new item insert. So we
		   reserve space for one extent unit. */
		hint->len = sizeof(extent40_t);
	} else {
		uint64_t ins_offset;
		uint64_t max_offset;

		/* Getting maximal real key */
		extent40_maxreal_key(place, &hint->maxkey);

		/* Getting insert pos */
		ins_offset = plug_call(hint->offset.plug->o.key_ops,
				       get_offset, &hint->offset);

		max_offset = plug_call(hint->maxkey.plug->o.key_ops,
				       get_offset, &hint->maxkey);

		if (max_offset > 0)
			max_offset++;
		
		/* Checking if insert key lies behind the insert point item
		   data. If so, we will perform further checks. */
		if (ins_offset + hint->count > max_offset) {
			uint32_t blksize = extent40_blksize(place);

			/* Getting last unit in order to check if we can expand
			   it and just add data to it. */
			extent40_t *extent = extent40_body(place) +
				(extent40_units(place) - 1);

			/* Check if last unit is allocated already */
			if (et40_get_start(extent) != EXTENT_UNALLOC_UNIT) {
				/* Unit is allocated, so we cannot mix it with
				   new data and thus reserve one more extent
				   unit for it in the case we write actual data,
				   not a hole. */
				if (hint->specific) {
					hint->len = sizeof(extent40_t);
				} else {
					/* Checking if hole size is more than
					   block size. */
					if (hint->count >= blksize &&
					    et40_get_start(extent) != EXTENT_SPARSE_UNIT)
					{
						hint->len = sizeof(extent40_t);
					}
				}
			} else {
				/* Unit is not allocated, so we need only check
				   for holes. */
				if (!hint->specific && hint->count >= blksize &&
				    et40_get_start(extent) != EXTENT_SPARSE_UNIT)
				{
					hint->len = sizeof(extent40_t);
				}
			}
		}
	}

	return 0;
}

/* Writes data to extent. */
static int64_t extent40_write_units(place_t *place, trans_hint_t *hint) {
	char *buff;
	uint32_t units;
	uint32_t blksize;
	
	key_entity_t key;
	aal_block_t *block;
	extent40_t *extent;

	uint64_t max_offset;
	uint64_t ins_offset;
	uint64_t uni_offset;
	uint32_t count, size;
	uint64_t block_offset;
	
	aal_assert("umka-2357", hint != NULL);
	aal_assert("umka-2356", place != NULL);

	/* Correcting insert point, as it may point to -1 (create new item) and
	   behind last unit (adding data at the end of item). */
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	buff = hint->specific;
	units = extent40_units(place);
	
	/* Calculating insert unit offset and insert offset. */
	extent40_fetch_key(place, &key);

	blksize = extent40_blksize(place);

	uni_offset = plug_call(key.plug->o.key_ops,
			       get_offset, &key);
	
	ins_offset = plug_call(hint->offset.plug->o.key_ops,
			       get_offset, &hint->offset);

	max_offset = plug_call(hint->maxkey.plug->o.key_ops,
			       get_offset, &hint->maxkey);

	if (max_offset > 0)
		max_offset++;

	/* Main loop until all data is written. */
	for (hint->bytes = 0, count = hint->count; count > 0;
	     count -= size)
	{
		uint32_t off;
		uint32_t room;

		/* Calculating size to be written this time. */
		room = blksize - (ins_offset % blksize);
		
		if ((size = count) > room)
			size = room;
		
		/* Block offset we will insert in. */
		block_offset = ins_offset - (ins_offset & (blksize - 1));

		/* Preparing key for getting data by it. */
		plug_call(key.plug->o.key_ops, set_offset, &key, block_offset);

		/* Checking if we write data inside item. */
		if (block_offset < max_offset) {
			blk_t blk;

			/* Getting data block by offset key. */
			if (!(block = extent40_core->tree_ops.get_data(hint->tree,
								       &key)))
			{
				/* This is the case, when data cache does not
				   contain needed block, we have to load it
				   before modifying. */
				extent = extent40_body(place) + place->pos.unit;

				blk = et40_get_start(extent) +
					(block_offset - uni_offset) / blksize;

				/* Loading data block. */
				if (!(block = aal_block_load(extent40_device(place),
							     blksize, blk)))
				{
					return -EIO;
				}

				/* Updating it data cache. */
				extent40_core->tree_ops.put_data(hint->tree,
								 &key, block);
			}
		} else {
			/* We write beyond of item and thus need to allocate new
			   blocks. */
			if (!(block = aal_block_alloc(extent40_device(place),
						      blksize, 0)))
			{
				return -ENOMEM;
			}

			/* Attaching new block to data cache. */
			extent40_core->tree_ops.put_data(hint->tree,
							 &key, block);
			
			/* Checking if we write data or holes. */
			if (hint->specific && max_offset) {
				extent = extent40_body(place) + place->pos.unit - 1;

				/* Setting up new units. */
				if (et40_get_start(extent) != EXTENT_UNALLOC_UNIT) {
					/* Previous unit is allocated one, thus
					   we cannot just enlarge it and need to
					   set up new unit. */
					et40_set_start(extent + 1,
						       EXTENT_UNALLOC_UNIT);
					
					et40_set_width(extent + 1, 1);
				} else {
					/* Enlarging previous unit, as it is
					   unallocated one. */
					et40_inc_width(extent, 1);
				}

				/* Updating counters. */
				hint->bytes += blksize;
				max_offset += blksize;
			} else {
				extent = extent40_body(place) +
					place->pos.unit;
				
				if (max_offset > 0)
					extent++;
					
				/* This is the case when we write holes */
				if (!hint->specific && count >= blksize &&
				    (ins_offset % blksize) == 0)
				{
					uint64_t width;
					
					/* Hole is bigger than @blksize and
					   we're on offset multiple of blksize,
					   so we can insert hole unit. */
					size = count - (count &
							(blksize - 1));

					et40_set_start(extent,
						       EXTENT_SPARSE_UNIT);

					width = (size / blksize);
					et40_set_width(extent, width);
				} else {
					/* Setting up new unallocated unit. */
					et40_set_start(extent,
						       EXTENT_UNALLOC_UNIT);
						
					et40_set_width(extent, 1);
					hint->bytes += blksize;
				}
				
				max_offset += blksize;
			}
		}

		/* Writting data to @block */
		if (hint->specific) {
			off = (ins_offset % blksize);
			aal_memcpy(block->data + off, buff, size);

			buff += size;
			block->dirty = 1;
		} else {
			/* Writting hole */
			if (size < blksize) {
				off = (ins_offset % blksize);
				aal_memset(block->data + off, 0, size);
				block->dirty = 1;
			}
		}

		ins_offset += size;
	}
	
	place_mkdirty(place);
	return hint->count;
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
static errno_t extent40_prep_shift(place_t *src_place,
				   place_t *dst_place,
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

			if (hint->pos.unit == 0 && hint->control & SF_MOVE_POINT) {
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
			/* Check is it is possible to move something into right
			   neighbour item. */
			if (hint->pos.unit * sizeof(extent40_t) < src_place->len) {
				right = src_place->len -
					(hint->pos.unit * sizeof(extent40_t));
		
				if (hint->units_bytes > right)
					hint->units_bytes = right;

				if (hint->control & SF_MOVE_POINT &&
				    hint->pos.unit == ((src_place->len - hint->units_bytes) /
						       sizeof(extent40_t)))
				{
					hint->result |= SF_MOVE_POINT;
					hint->pos.unit = 0;
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

	hint->units_number = hint->units_bytes / sizeof(extent40_t);
	return 0;
}

static errno_t extent40_shift_units(place_t *src_place, place_t *dst_place,
				    shift_hint_t *hint)
{
	uint32_t pos;
	uint64_t offset;
	
	aal_assert("umka-1708", hint != NULL);
	aal_assert("umka-1706", src_place != NULL);
	aal_assert("umka-1707", dst_place != NULL);

	if (hint->control & SF_ALLOW_LEFT) {
		pos = extent40_units(dst_place) -
			hint->units_number;
			
		/* Preparing space in @dst_place */
		extent40_expand(dst_place, pos, hint->units_number);

		/* Copying data from the @src_place to @dst_place */
		extent40_copy(dst_place, pos, src_place, 0,
			      hint->units_number);

		/* Removing units in @src_place */
		extent40_shrink(src_place, 0, hint->units_number);

		/* Updating item's key by the first unit key */
		body40_get_key(src_place, hint->units_number,
			       &src_place->key, extent40_offset);
	} else {
		/* Preparing space in @dst_place */
		extent40_expand(dst_place, 0, hint->units_number);

		/* Copying data from the @src_place to @dst_place */
		pos = extent40_units(src_place) - hint->units_number;
		
		extent40_copy(dst_place, 0, src_place, pos,
			      hint->units_number);

		/* Removing units in @src_place */
		extent40_shrink(src_place, pos, hint->units_number);

		/* Updating item's key by the first unit key */
		body40_get_key(dst_place, 0, &dst_place->key,
			       extent40_offset);

		offset = plug_call(dst_place->key.plug->o.key_ops,
				   get_offset, &dst_place->key);

		offset -= extent40_offset(dst_place, hint->units_number);

		plug_call(dst_place->key.plug->o.key_ops,
			  set_offset, &dst_place->key, offset);
	}
	
	return 0;
}
#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_STAND_ALONE
	.fuse		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = extent40_mergeable,
	.prep_shift	  = extent40_prep_shift,
	.shift_units	  = extent40_shift_units,
	.maxreal_key	  = extent40_maxreal_key,
#endif
	.units		  = extent40_units,
	.lookup		  = extent40_lookup,
	.fetch_key	  = extent40_fetch_key,
	.maxposs_key      = extent40_maxposs_key
};

static item_object_ops_t object_ops = {
	.object_plug	  = NULL,
#ifndef ENABLE_STAND_ALONE
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
#endif
	.read_units	  = extent40_read_units,
	.fetch_units	  = extent40_fetch_units
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE
	.check_layout	  = extent40_check_layout,
	.check_struct	  = extent40_check_struct,
	
	.prep_merge	  = extent40_prep_merge,
	.merge		  = extent40_merge,

	.pack		  = NULL,
	.unpack		  = NULL
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE
	.print		  = extent40_print,
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link	  = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link	  = NULL,
#endif
};

static reiser4_item_ops_t extent40_ops = {
	.tree		  = &tree_ops,
	.debug		  = &debug_ops,
	.object		  = &object_ops,
	.repair		  = &repair_ops,
	.balance	  = &balance_ops
};

static reiser4_plug_t extent40_plug = {
	.cl    = class_init,
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
	extent40_core = c;
	return &extent40_plug;
}

plug_register(extent40, extent40_start, NULL);
