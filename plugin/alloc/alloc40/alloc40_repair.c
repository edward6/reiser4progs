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
	region_func_t func;
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
		hint->func(entity, start, width, hint->data);
	
	return 0;
}

errno_t alloc40_layout_bad(generic_entity_t *entity,
			   region_func_t func, void *data)
{
	struct alloc_hint hint;
	
	aal_assert("vpf-1323", func != NULL);
	
	hint.func = func;
	hint.data = data;
	
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

	chunk = (map + alloc->bitmap->size) - current > (int)size ? 
		(int)size : (int)((map + alloc->bitmap->size) - current);

	/* Write checksum. */
	aal_stream_write(stream, alloc->crc + (offset * CRC_SIZE),
			 CRC_SIZE);

	/* Write bitmap. */
	aal_stream_write(stream, current, size);
	
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

	offset = start / size / 8;
	size = alloc->blksize - CRC_SIZE;
	map = aux_bitmap_map(alloc->bitmap);
	current = map + (size * (start / size / 8));

	chunk = (map + alloc->bitmap->size) - current > (int)size ? 
		(int)size : (int)((map + alloc->bitmap->size) - current);

	/* Write checksum. */
	aal_stream_read(stream, alloc->crc + (offset * CRC_SIZE),
			CRC_SIZE);

	/* Write bitmap. */
	aal_stream_read(stream, current, size);
	
	return 0;
}

#define ALLOC40_SIGN "AL40"

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

	aal_stream_write(stream, ALLOC40_SIGN, 4);

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
generic_entity_t *alloc40_unpack(aal_device_t *device,
				 uint32_t blksize,
				 aal_stream_t *stream)
{
	uint64_t len;
	uint32_t crcsize;
	alloc40_t *alloc;
	char sign[5] = {0};
	
	aal_assert("umka-2620", device != NULL);
	aal_assert("umka-2621", stream != NULL);

	/* Check signature first. */
	aal_stream_read(stream, sign, 4);
	
	if (aal_strncmp(sign, ALLOC40_SIGN, 4)) {
		aal_exception_error("Invalid block allocator magic "
				    "%s is detected in stream.", sign);
		return NULL;
	}

	/* Allocating block allocator instance and initializing it by passed
	   @blksize, @device and data from the @stream. */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->device = device;
	alloc->blksize = blksize;
	alloc->plug = &alloc40_plug;

	/* Read number of bits in bitmap. */
	aal_stream_read(stream, &len, sizeof(len));

	if (!(alloc->bitmap = aux_bitmap_create(len)))
		goto error_free_alloc;

	/* Initializing adler checksums. */
	crcsize = (alloc->bitmap->size /
		   (alloc->blksize - CRC_SIZE)) * CRC_SIZE;
    
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
