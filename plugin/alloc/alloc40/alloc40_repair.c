/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.c -- repair default block allocator plugin methods. */

#ifndef ENABLE_MINIMAL

#include "alloc40.h"
#include <repair/plugin.h>

typedef struct alloc40_hint {
	alloc40_t *alloc;
	region_func_t func;
	void *data;
} alloc40_hint_t;

extern errno_t cb_valid_block(blk_t start, count_t width, void *data);

static errno_t cb_check_layout(blk_t start, count_t width, void *data) {
	alloc40_hint_t *hint = (alloc40_hint_t *)data;
	errno_t res;

	res = cb_valid_block(start, width, hint->alloc);
	
	if (res && res != -ESTRUCT)
		return res;
	
	/* If bitmap block looks corrupted or the very first bit 
	   is not set, call func for the region. */
	if (res || !alloc40_occupied((generic_entity_t *)hint->alloc,
				     start, 1))
	{
		if ((res = hint->func(start, width, hint->data)))
			return res;
	}
	
	return 0;
}

errno_t alloc40_layout_bad(generic_entity_t *entity,
			   region_func_t func, void *data)
{
	alloc40_hint_t hint;
	
	aal_assert("umka-2646", entity != NULL);
	aal_assert("vpf-1323", func != NULL);
	
	hint.alloc = (alloc40_t *)entity;
	hint.alloc->data = NULL;
	hint.func = func;
	hint.data = data;
	
	return alloc40_layout(entity, cb_check_layout, &hint);
}

/* Callback for packing one bitmap block. */
static errno_t cb_pack_bitmap(blk_t start, count_t width, void *data) {
	uint32_t size;
	uint32_t chunk;
	uint32_t offset;
	
	alloc40_t *alloc;
	char *map, *current;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)data;
	stream = (aal_stream_t *)alloc->data;

	size = alloc->blksize - CRC_SIZE;
	offset = start / size / 8;

	map = alloc->bitmap->map;
	current = map + (size * (start / size / 8));

	if ((chunk = (map + alloc->bitmap->size) - current) > size)
		chunk = size;

	aal_stream_write(stream, alloc->crc + (offset * CRC_SIZE),
			 CRC_SIZE);

	aal_stream_write(stream, current, chunk);
	
	return 0;
}

/* Callback for unpacking one bitmap block. */
static errno_t cb_unpack_bitmap(blk_t start, count_t width, void *data)
{
	uint32_t size;
	uint32_t chunk;
	uint32_t offset;
	
	alloc40_t *alloc;
	char *map, *current;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)data;
	stream = (aal_stream_t *)alloc->data;

	size = alloc->blksize - CRC_SIZE;
	offset = start / size / 8;
	
	map = alloc->bitmap->map;
	current = map + (size * (start / size / 8));

	if ((chunk = (map + alloc->bitmap->size) - current) > size)
		chunk = size;

	if (aal_stream_read(stream, alloc->crc + (offset * CRC_SIZE),
			    CRC_SIZE) != CRC_SIZE)
	{
		aal_error("Can't unpack the bitmap block (%llu)."
			  "Steam is over?", start);
		return -EIO;
	}

	if (aal_stream_read(stream, current, chunk) != (int32_t)chunk) {
		aal_error("Can't unpack the bitmap block (%llu)."
			  "Steam is over?", start);
		return -EIO;
	}
	
	return 0;
}

/* Pack block allocator data to passed @stream. */
errno_t alloc40_pack(generic_entity_t *entity,
		     aal_stream_t *stream)
{
	errno_t res;
	uint64_t len;
	alloc40_t *alloc;
	
	aal_assert("umka-2618", entity != NULL);
	aal_assert("umka-2619", stream != NULL);

	alloc = (alloc40_t *)entity;

	len = alloc->bitmap->total;
	aal_stream_write(stream, &len, sizeof(len));

	/* Calling layout() function for packing all bitmap blocks. */
	alloc->data = stream;
	if ((res = alloc40_layout(entity, cb_pack_bitmap, alloc))) {
		aal_error("Can't pack bitmap.");
		return res;
	}

	return 0;
}

/* Create block allocator from passed @stream. */
generic_entity_t *alloc40_unpack(aal_device_t *device, 
				 uint32_t blksize,
				 aal_stream_t *stream)
{
	uint64_t blocks;
	uint32_t crcsize;
	uint32_t mapsize;
	alloc40_t *alloc;
	
	aal_assert("umka-2620", device != NULL);
	aal_assert("umka-2621", stream != NULL);

	/* Allocating block allocator instance and initializing it by passed
	   @desc and data from the @stream. */
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	alloc->plug = &alloc40_plug;
	alloc->device = device;
	alloc->blksize = blksize;

	/* Read number of bits in bitmap. */
	if (aal_stream_read(stream, &blocks, sizeof(blocks)) != sizeof(blocks))
	{
		aal_error("Can't unpack the bitmap. Steam is over?");
		goto error_free_alloc;
	}

	if (!(alloc->bitmap = reiser4_bitmap_create(blocks)))
		goto error_free_alloc;

	/* Initializing adler checksums. */
	mapsize = alloc->blksize - CRC_SIZE;

	crcsize = ((alloc->bitmap->size + mapsize - 1) /
		   mapsize) * CRC_SIZE;
    
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
	
	/* Calling layout() function for reading all bitmap blocks to
	   @alloc->bitmap. */
	alloc->data = stream;
	if (alloc40_layout((generic_entity_t *)alloc, 
			   cb_unpack_bitmap, alloc))
	{
		aal_error("Can't unpack bitmap.");
		goto error_free_crc;
	}

	alloc->state = (1 << ENTITY_DIRTY);
	reiser4_bitmap_calc_marked(alloc->bitmap);
	
	return (generic_entity_t *)alloc;

 error_free_crc:
	aal_free(alloc->crc);
 error_free_bitmap:
	reiser4_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

static errno_t cb_print_bitmap(blk_t start, count_t width, void *data) {
	uint64_t offset, end;
	uint64_t i, count;
	uint32_t size;

	alloc40_t *alloc;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)data;
	stream = (aal_stream_t *)alloc->data;

	size = (alloc->blksize - CRC_SIZE) * 8;
	offset = start / size;
	end = (offset  + 1) * size;
	end = end <= alloc->bitmap->total ? end : alloc->bitmap->total;

	count = 0;
	for (i = offset * size; i < end; i++)
		count += (aal_test_bit(alloc->bitmap->map, i) ? 1 : 0);
	
	aal_stream_format(stream, "%*llu [ 0x%lx ] %llu\n", 10, start,
			  *((uint32_t *)(alloc->crc + offset * CRC_SIZE)), count);

	return 0;
}

/* Handler for "print" method. */
void alloc40_print(generic_entity_t *entity, 
		   aal_stream_t *stream, 
		   uint16_t options)
{
	uint64_t start;
	uint64_t total;
	uint64_t blocks;
	alloc40_t *alloc;
	
	aal_assert("umka-1778", entity != NULL);
	aal_assert("umka-1779", stream != NULL);

	alloc = (alloc40_t *)entity;

	/* Printing into passed @stream block allocator properties. Here also
	   will be printing of the bitmap bits here later. */
	aal_stream_format(stream, "Block allocator:\n");
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  alloc->plug->label);

	aal_stream_format(stream, "total blocks:\t%llu\n",
			  alloc->bitmap->total);

	aal_stream_format(stream, "used blocks:\t%llu\n",
			  alloc->bitmap->marked);

	aal_stream_format(stream, "free blocks:\t%llu\n",
			  alloc->bitmap->total -
			  alloc->bitmap->marked);

	aal_stream_format(stream, "\n%*s%*s%*s\n", 10, "BLK", 10, "CRC", 10, "Used");
	aal_stream_format(stream, "-------------------------\n");

	/* Calling alloc40_layout() in order to print all block checksums */
	alloc->data = stream;
	alloc40_layout((generic_entity_t *)alloc, 
		       cb_print_bitmap, alloc);
	
	start = 0;
	total = alloc->bitmap->total;

	aal_stream_format(stream, "\nBlock map:\n");
	
	aal_stream_format(stream, "[ ");

	while (start < total) {
		blocks = reiser4_bitmap_find_region(alloc->bitmap, &start,  
						    total - start, 1);
		if (!blocks) 
			break;

		aal_stream_format(stream, "%llu(%llu) ", start, blocks);
		start += blocks;
	}
	
	aal_stream_format(stream, "]\n");
}


static void cb_inval_warn(blk_t start, uint32_t ladler, uint32_t cadler) {
	fsck_mess("Checksum mismatch in bitmap block %llu. Checksum "
		  "is 0x%x, should be 0x%x.", start, ladler, cadler);
}

/* Checks allocator on validness  */
errno_t alloc40_check_struct(generic_entity_t *entity, uint8_t mode) {
	alloc40_t *alloc = (alloc40_t *)entity;
	errno_t res;

	aal_assert("umka-963", alloc != NULL);
	aal_assert("umka-964", alloc->bitmap != NULL);

	/* Calling layout function for traversing all the bitmap blocks with
	   checking callback function. */
	alloc->data = cb_inval_warn;
	res = alloc40_layout((generic_entity_t *)alloc,
			     cb_valid_block, alloc);

	if (res != -ESTRUCT)
		return res;

	/* Checksums are not correct. */
	if (mode == RM_CHECK)
		return RE_FIXABLE;

	alloc->state = (1 << ENTITY_DIRTY);
	fsck_mess("Checksums will be fixed later.");
	return 0;
}


#endif
