/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.c -- repair default block allocator plugin methods. */

#ifndef ENABLE_STAND_ALONE
#include "alloc40.h"
#include <repair/plugin.h>

/* Call @func for all blocks which belong to the same bitmap block as passed
   @blk. It is needed for fsck. In the case it detremined that a block is not
   corresponds to its value in block allocator, it should check all the related
   (neighbour) blocks which are described by one bitmap block (4096 - CRC_SIZE).
*/
errno_t alloc40_region(generic_entity_t *entity, blk_t blk, 
		       region_func_t region_func, void *data) 
{
	uint64_t size;
	alloc40_t *alloc;
    
	aal_assert("vpf-554", entity != NULL);
	aal_assert("umka-1746", region_func != NULL);
    
	alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-710", alloc->bitmap != NULL);
	aal_assert("vpf-711", alloc->device != NULL);
    
	size = alloc->blksize - CRC_SIZE;
    
	/* Loop though the all blocks one bitmap block describes and calling
	   passed @region_func for each of them. */   
	return region_func(entity, (blk / size) * size, size, data);
}

struct alloc_hint {
	region_func_t region_func;
	void *data;
};

static errno_t callback_check_layout(void *entity, blk_t start, 
				     count_t width, void *data) 
{
	struct alloc_hint *hint = (struct alloc_hint *)data;
	errno_t res;
	
	if ((res = callback_valid(entity, start, width, NULL)) < 0)
		return res;
	
	/* If bitmap block looks corrupted or the very first bit is not set,
	   call func for the region */
	if (res || alloc40_occupied(entity, start, 1))
		hint->region_func(entity, start, width, hint->data);
	
	return 0;
}

errno_t alloc40_layout_bad(generic_entity_t *entity,
			   region_func_t region_func,
			   void *data)
{
	struct alloc_hint hint;
	
	aal_assert("umka-2646", entity != NULL);
	aal_assert("vpf-1323", region_func != NULL);
	
	hint.data = data;
	hint.region_func = region_func;
	
	return alloc40_layout(entity, callback_check_layout, &hint);
}

/* Callback for packing one bitmap block. */
static errno_t callback_pack_bitmap(void *entity, blk_t start,
				    count_t width, void *data)
{
	uint32_t size;
	uint32_t chunk;
	uint32_t offset;
	
	alloc40_t *alloc;
	char *map, *current;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)entity;
	stream = (aal_stream_t *)data;

	size = alloc->blksize - CRC_SIZE;
	offset = start / size / 8;

	map = aux_bitmap_map(alloc->bitmap);
	current = map + (size * (start / size / 8));

	if ((chunk = (map + alloc->bitmap->size) - current) > size)
		chunk = size;

	aal_stream_write(stream, alloc->crc + (offset * CRC_SIZE),
			 CRC_SIZE);

	aal_stream_write(stream, current, chunk);
	
	return 0;
}

/* Callback for unpacking one bitmap block. */
static errno_t callback_unpack_bitmap(void *entity, blk_t start,
				      count_t width, void *data)
{
	uint32_t size;
	uint32_t chunk;
	uint32_t offset;
	
	alloc40_t *alloc;
	char *map, *current;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)entity;
	stream = (aal_stream_t *)data;

	size = alloc->blksize - CRC_SIZE;
	offset = start / size / 8;
	
	map = aux_bitmap_map(alloc->bitmap);
	current = map + (size * (start / size / 8));

	if ((chunk = (map + alloc->bitmap->size) - current) > size)
		chunk = size;

	aal_stream_read(stream, alloc->crc + (offset * CRC_SIZE),
			CRC_SIZE);

	aal_stream_read(stream, current, chunk);
	
	return 0;
}

/* Pack block allocator data to passed @stream. */
errno_t alloc40_pack(generic_entity_t *entity,
		     aal_stream_t *stream)
{
	rid_t pid;
	errno_t res;
	uint64_t len;
	alloc40_t *alloc;
	
	aal_assert("umka-2618", entity != NULL);
	aal_assert("umka-2619", stream != NULL);

	alloc = (alloc40_t *)entity;

	pid = entity->plug->id.id;
	aal_stream_write(stream, &pid, sizeof(pid));

	len = alloc->bitmap->total;
	aal_stream_write(stream, &len, sizeof(len));

	/* Calling layout() function for packing all bitmap blocks. */
	if ((res = alloc40_layout(entity, callback_pack_bitmap, stream))) {
		aal_exception_error("Can't pack bitmap.");
		return res;
	}

	return 0;
}

/* Create block allocator from passed @stream. */
generic_entity_t *alloc40_unpack(fs_desc_t *desc,
				 aal_stream_t *stream)
{
	uint64_t blocks;
	uint32_t crcsize;
	uint32_t mapsize;
	alloc40_t *alloc;
	
	aal_assert("umka-2620", desc != NULL);
	aal_assert("umka-2621", stream != NULL);

	/* Allocating block allocator instance and initializing it by passed
	   @desc and data from the @stream. */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->plug = &alloc40_plug;
	alloc->device = desc->device;
	alloc->blksize = desc->blksize;

	/* Read number of bits in bitmap. */
	aal_stream_read(stream, &blocks, sizeof(blocks));

	if (!(alloc->bitmap = aux_bitmap_create(blocks)))
		goto error_free_alloc;

	/* Initializing adler checksums. */
	mapsize = alloc->blksize - CRC_SIZE;

	crcsize = ((alloc->bitmap->size + mapsize - 1) /
		   mapsize) * CRC_SIZE;
    
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
	
	/* Calling layout() function for reading all bitmap blocks to
	   @alloc->bitmap. */
	if (alloc40_layout((generic_entity_t *)alloc,
			   callback_unpack_bitmap, stream))
	{
		aal_exception_error("Can't unpack bitmap.");
		goto error_free_crc;
	}

	alloc->dirty = 1;
	aux_bitmap_calc_marked(alloc->bitmap);
	
	return (generic_entity_t *)alloc;

 error_free_crc:
	aal_free(alloc->crc);
 error_free_bitmap:
	aux_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}
#endif
