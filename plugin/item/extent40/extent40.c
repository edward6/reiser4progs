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
		aal_error("Invalid extent item size (%u) detected. "
			  "Node %llu, item %u.", place->len,
			  place->block->nr, place->pos.item);
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
    
	return (blocks * extent40_blksize(place));
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
                width += et40_get_width(extent) *
			extent40_blksize(place);

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
	aal_assert("vpf-438", key  != NULL);
	aal_assert("vpf-437", place != NULL);

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
		extent40_t *extent;
		uint64_t blk, start;

		extent = extent40_body(place);

		/* Calculating start block for read. */
		start = blk = et40_get_start(extent + i) +
			(rel_offset / blksize);

		if (start == EXTENT_HOLE_UNIT) {
			/* Handle the holes. Here we fill @buff by zeros, as
			   hole is detected during read. */
			uint64_t width = et40_get_width(extent + i);
			
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
	extent40_expand(place, place->pos.unit,
			hint->count);

	/* Updating @count units at @place */
	return extent40_update_units(place, hint);
}

/* Estimates extent write operation */
static errno_t extent40_prep_write(place_t *place,
				   trans_hint_t *hint)
{
	uint32_t units;
	key_entity_t key;

	uint32_t blksize;
	uint32_t unit_pos;

	extent40_t *extent;
	uint32_t count, size;

	uint64_t uni_offset;
	uint64_t ins_offset;
	uint64_t max_offset;

	aal_assert("umka-1836", hint != NULL);
	aal_assert("umka-2425", place != NULL);

	hint->len = 0;
	hint->overhead = 0;
		
	if (place->pos.unit == MAX_UINT32) {
		/* Assigning maxkey to key of new created item. */
		plug_call(hint->offset.plug->o.key_ops,
			  assign, &hint->maxkey, &hint->offset);
		
		/* Insert point is -1, thus, this is new item insert. So we
		   reserve space for one extent unit. */
		hint->len = sizeof(extent40_t);
	} else {
		unit_pos = place->pos.unit;
		units = extent40_units(place);

		extent40_fetch_key(place, &key);
		blksize = extent40_blksize(place);

		/* Getting maximal real key. It will be needed to determine if
		   we insert data inside extent or behind it. */
		extent40_maxreal_key(place, &hint->maxkey);

		if ((max_offset = plug_call(hint->maxkey.plug->o.key_ops,
					    get_offset, &hint->maxkey)) > 0)
		{
			max_offset++;
		}

		plug_call(hint->maxkey.plug->o.key_ops,
			  set_offset, &hint->maxkey, max_offset);

		/* Getting unit offset amd insert offset. They both are used
		   during estimation. */
		uni_offset = plug_call(key.plug->o.key_ops,
				       get_offset, &key);
	
		ins_offset = plug_call(hint->offset.plug->o.key_ops,
				       get_offset, &hint->offset);

		/* This loop checks if we insert some data inside extent, we
		   should take into account posible holes. */
		for (count = hint->count; count > 0 && unit_pos < units;
		     count -= size, unit_pos++)
		{
			uint64_t unit_size;
		
			extent = extent40_body(place) + unit_pos;
			unit_size = et40_get_width(extent) * blksize;
		
			if ((size = unit_size - (ins_offset - uni_offset)) > count)
				size = count;

			if (et40_get_start(extent) == EXTENT_HOLE_UNIT)	{
				/* We will allocate new unit if we write data to
				   hole and data size less than @unit_size. */
				if (hint->specific && count < unit_size)
					hint->len += sizeof(extent40_t);
			}

			ins_offset += size;
			uni_offset += unit_size;
		}

		/* This is a handling of the case when the rest of data should
		   be written to extent. It take care about the cases, when we
		   need to append some data at the end of extent. */
		if (count > 0) {
			extent = extent40_body(place) + units - 1;
		
			/* Check if last unit is allocated already. */
			if (et40_get_start(extent) == EXTENT_UNALLOC_UNIT) {
				/* Unit is not allocated yet, so we need only
				   check for holes. */
				if (!hint->specific && count >= blksize)
					hint->len += sizeof(extent40_t);
			} else {
				/* Unit is allocated one or hole. */
				if (et40_get_start(extent) == EXTENT_HOLE_UNIT) {
					if (hint->specific || count < blksize)
						hint->len += sizeof(extent40_t);
				} else {
					hint->len += sizeof(extent40_t);
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
	uint32_t unit_pos;

	key_entity_t key;
	aal_block_t *block;
	extent40_t *extent;

	uint64_t max_offset;
	uint64_t ins_offset;
	uint64_t uni_offset;
	uint32_t count, size;
	aal_device_t *device;
	uint64_t block_offset;
	
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
	
	/* Calculating insert unit offset and insert offset. */
	extent40_fetch_key(place, &key);
	blksize = extent40_blksize(place);

	/* Getting unit offset and insert offset. They both are used during
	   writing the data. */
	uni_offset = plug_call(key.plug->o.key_ops,
			       get_offset, &key);
	
	ins_offset = plug_call(hint->offset.plug->o.key_ops,
			       get_offset, &hint->offset);

	/* Getting max real item offset. It is used to determine if we have to
	   write inside extent or behind it. */
	max_offset = plug_call(hint->maxkey.plug->o.key_ops,
			       get_offset, &hint->maxkey);

	/* Allocate new unit stage. Here we will set new unit up. */
	if (ins_offset + hint->count <= max_offset) {
		/* Writing inside item. Here we should handle the case of
		   overwriting hole units. */
		extent = extent40_body(place) + unit_pos;
		
		if (et40_get_start(extent) == EXTENT_HOLE_UNIT)	{
			uint64_t unit_size;

			unit_size = et40_get_width(extent) * blksize;
			
			/* We will allocate new unit if we write data to hole
			   and data size less than @unit_size. */
			if (hint->specific && hint->count < unit_size) {
				aal_error("Sorry, not implemented yet!");
				return -EINVAL;
			}
		}
	} else {
		uint32_t width;
		uint64_t towrite;
		int allocate = 0;

		/* Getting extent unit to be set fully (start and width) or only
		   width. */
		extent = extent40_body(place) + units - 1;
		towrite = hint->count - (max_offset - ins_offset);

		/* Calculating width in blocks of new unit to be inserted. */
		width = (towrite + blksize - 1) / blksize;
		
		/* Check if we have to write first unit in item or not. */
		if (max_offset > 0 && (units != 1 || hint->len == 0)) {
			extent40_t *last;

			/* We write inside existent item and have to check its
			   last unit, in order to know if we can enlarge it. */
			last = extent40_body(place) + units - 1 -
				(hint->len ? 1 : 0);
			
			if (et40_get_start(last) == EXTENT_UNALLOC_UNIT) {
				/* Last unit is unallocated one, thus, we insert
				   not a hole, we will enlarge it. And we will
				   allocate new unit in the case of writing
				   holes. */
				if (!hint->specific && towrite >= blksize) {
					aal_assert("umka-3065", hint->len);
					
					et40_set_start(extent,
						       EXTENT_HOLE_UNIT);

					et40_set_width(extent, width);
				} else {
					allocate = 1;
					et40_inc_width(extent, width);
				}
			} else {
				/* Last unit is not unallocated one. That is
				   it's a hole or usual data unit.*/
				if (et40_get_start(last) == EXTENT_HOLE_UNIT) {
					/* Check if last unit is a hole. If so
					   and we write data or hole of size
					   less than blksize, we will insert new
					   unit at the end of extent. */
					if (hint->specific || towrite < blksize) {
						aal_assert("umka-3066", hint->len);
							
						allocate = 1;
							
						et40_set_start(extent,
							       EXTENT_UNALLOC_UNIT);
					
						et40_set_width(extent, width);
					} else {
						/* Unit may be just enlarged. */
						et40_inc_width(extent, width);
					}
				} else {
					aal_assert("umka-3067", hint->len);

					/* Last unit is allocated one. Check if
					   we write a hole and if so, set start
					   of new unit to zero. Set it to
					   unallocated unit otherwise. */
					if (!hint->specific && towrite >= blksize) {
						et40_set_start(extent,
							       EXTENT_HOLE_UNIT);
					} else {
						allocate = 1;
							
						et40_set_start(extent,
							       EXTENT_UNALLOC_UNIT);
					}
					
					et40_set_width(extent, width);
				}
			}
		} else {
			/* Handling the case when no item yet exists. Forminng
			   first unit. */
			if (hint->specific || towrite < blksize) {
				allocate = 1;
					
				et40_set_start(extent,
					       EXTENT_UNALLOC_UNIT);
					
			} else {
				et40_set_start(extent,
					       EXTENT_HOLE_UNIT);
			}

			et40_set_width(extent, width);
		}

		max_offset += (width * blksize);
		
		/* Allocating new block if needed and attaching it to data
		   cache. */
		if (allocate) {
			uint64_t offset;

			/* Adjusting offset of first block to make it pointed to
			   the border of block. */
			offset = plug_call(hint->offset.plug->o.key_ops,
					   get_offset, &hint->offset);

			offset = (offset + (hint->count % blksize)) -
				((offset + (hint->count % blksize)) &
				 (blksize - 1));

			plug_call(key.plug->o.key_ops, set_offset, &key, offset);
			
			for (; width > 0; width--) {
				
				if (!(block = aal_block_alloc(device, blksize, 0)))
					return -ENOMEM;

				extent40_core->tree_ops.put_data(hint->tree,
								 &key, block);

				/* Update @key offset. */
				offset = plug_call(key.plug->o.key_ops,
						   get_offset, &key);

				plug_call(key.plug->o.key_ops, set_offset,
					  &key, offset + blksize);

				/* Updating @hint->bytes field by blksize, as
				   new block is allocated. */
				hint->bytes += blksize;
			}
		}
	}

	/* Updating @key by unit key as it is changed. */
	extent40_fetch_key(place, &key);

	/* Second stage -- writing data to allocated blocks. */
	for (count = hint->count; count > 0; count -= size) {
		uint32_t off;
		uint32_t room;

		/* Calculating size to be written this time. */
		room = blksize - (ins_offset % blksize);
		
		if ((size = count) > room)
			size = room;

		/* Block offset we will insert in. */
		block_offset = ins_offset - (ins_offset & (blksize - 1));

		/* Preparing key for getting data by it. */
		plug_call(key.plug->o.key_ops, set_offset, &key,
			  block_offset);

		/* Checking if we write data inside item. */
		if (block_offset < max_offset) {
			uint64_t width;
			
			extent = extent40_body(place) + unit_pos;
			width = et40_get_width(extent);

			if (et40_get_start(extent) == EXTENT_HOLE_UNIT) {
			} else {
				/* Getting data block by offset key. Block
				   should be get before modifying it. */
				if (!(block = extent40_core->tree_ops.get_data(hint->tree,
									       &key)))
				{
					blk_t blk;
				
					if (et40_get_start(extent) == EXTENT_UNALLOC_UNIT) {
						aal_error("Unallocated extent unit without "
							  "attached block.");
						return -EIO;
					}
					
					blk = et40_get_start(extent) +
						(block_offset - uni_offset) / blksize;

					/* Loading data block. */
					if (!(block = aal_block_load(device,
								     blksize, blk)))
					{
						aal_error("Can't read block %llu. %s.",
							  blk, device->error);
						return -EIO;
					}

					/* Updating block in data cache. */
					extent40_core->tree_ops.put_data(hint->tree,
									 &key, block);
				}

				/* Writting data to @block. */
				if (hint->specific) {
					aal_assert("umka-3068", block != NULL);
			
					off = (ins_offset % blksize);
					aal_memcpy(block->data + off, buff, size);

					buff += size;
					block->dirty = 1;
				} else {
					/* Writting a hole. */
					if (size < blksize) {
						aal_assert("umka-3069", block != NULL);
				
						off = (ins_offset % blksize);
						aal_memset(block->data + off, 0, size);
						block->dirty = 1;
					}
				}
			}

			if (((ins_offset + size) - uni_offset) / blksize == width) {
				unit_pos++;
				uni_offset += width * blksize;
			}
		} else {
			aal_bug("umka-3072", "block_offset: %llu, max_offset: %llu",
				block_offset, max_offset);
		}

		ins_offset += size;
	}
	
	place_mkdirty(place);
	return hint->count - count;
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

static errno_t extent40_shift_units(place_t *src_place, place_t *dst_place,
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

		/* Removing units in @src_place. */
		extent40_shrink(src_place, 0, hint->units_number);

		/* Updating item's key by the first unit key. */
		body40_get_key(src_place, hint->units_number,
			       &src_place->key, extent40_offset);
	} else {
		/* Preparing space in @dst_place */
		extent40_expand(dst_place, 0, hint->units_number);

		/* Copying data from the @src_place to @dst_place. */
		pos = extent40_units(src_place) - hint->units_number;
		
		extent40_copy(dst_place, 0, src_place, pos,
			      hint->units_number);

		/* Removing units in @src_place. */
		extent40_shrink(src_place, pos, hint->units_number);

		/* Updating item's key by the first unit key. */
		body40_get_key(src_place, pos, &dst_place->key,
			       extent40_offset);
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
