/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.c -- repair default block allocator plugin methods. */

#ifndef ENABLE_MINIMAL

#include "alloc40.h"
#include <repair/plugin.h>

struct alloc_hint {
	region_func_t region_func;
	void *data;
};

extern errno_t cb_valid_block(void *entity, blk_t start,
			      count_t width, void *data);

static errno_t cb_check_layout(void *entity, blk_t start, 
			       count_t width, void *data) 
{
	struct alloc_hint *hint = (struct alloc_hint *)data;
	errno_t res;

	res = cb_valid_block(entity, start, width, NULL);
	
	if (res && res != -ESTRUCT)
		return res;
	
	/* If bitmap block looks corrupted or the very first bit 
	   is not set, call func for the region. */
	if (res || !alloc40_occupied(entity, start, 1)) {
		if ((res = hint->region_func(entity, start, width, 
					     hint->data)))
			return res;
	}
	
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
	
	return alloc40_layout(entity, cb_check_layout, &hint);
}

/* Callback for packing one bitmap block. */
static errno_t cb_pack_bitmap(void *entity, blk_t start,
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
static errno_t cb_unpack_bitmap(void *entity, blk_t start,
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
	if ((res = alloc40_layout(entity, cb_pack_bitmap, stream))) {
		aal_error("Can't pack bitmap.");
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
	if (aal_stream_read(stream, &blocks, sizeof(blocks)) != sizeof(blocks))
	{
		aal_error("Can't unpack the bitmap. Steam is over?");
		goto error_free_alloc;
	}

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
			   cb_unpack_bitmap, stream))
	{
		aal_error("Can't unpack bitmap.");
		goto error_free_crc;
	}

	alloc->state = (1 << ENTITY_DIRTY);
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

static errno_t cb_print_bitmap(void *entity, blk_t start,
			       count_t width, void *data)
{
	uint32_t size;
	uint64_t offset;
	alloc40_t *alloc;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)entity;
	stream = (aal_stream_t *)data;

	size = alloc->blksize - CRC_SIZE;
	offset = start / size / 8;
	
	aal_stream_format(stream, "%*llu [ 0x%lx ]\n", 10, start,
			  *((uint32_t *)alloc->crc + offset));

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

	aal_stream_format(stream, "\n%*s CRC\n", 10, "BLK");
	aal_stream_format(stream, "-------------------------\n");

	/* Calling alloc40_layout() in order to print all block checksums */
	alloc40_layout((generic_entity_t *)alloc, 
		       cb_print_bitmap, stream);
	
	start = 0;
	total = alloc->bitmap->total;

	aal_stream_format(stream, "\nBlock map:\n");
	
	aal_stream_format(stream, "[ ");

	while (start < total) {
		if (!(blocks = aux_bitmap_find_region(alloc->bitmap, &start,
						      total - start, 1)))
			break;

		aal_stream_format(stream, "%llu(%llu) ", start, blocks);
		start += blocks;
	}
	
	aal_stream_format(stream, "]\n");
}


static void cb_inval_warn(blk_t start, uint32_t ladler, uint32_t cadler) {
	fsck_mess("Checksum missmatch in bitmap block %llu. Checksum "
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
	res = alloc40_layout((generic_entity_t *)alloc,
			     cb_valid_block, cb_inval_warn);

	if (res != -ESTRUCT)
		return res;

	/* Checksums are not correct. */
	if (mode == RM_CHECK)
		return RE_FIXABLE;

	fsck_mess("Checksums will be fixed later.");
	return 0;
}


#endif
