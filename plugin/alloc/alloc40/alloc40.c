/*
  alloc40.c -- Default block allocator plugin for reiser4.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "alloc40.h"

#define CRC_SIZE (4)
#define ALLOC40_START (MASTER_OFFSET + (4096 * 2))

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t alloc40_plugin;

/* Call func for all blocks which belong to the same bitmap block as blk. */
errno_t alloc40_region(object_entity_t *entity, blk_t blk, 
		       block_func_t func, void *data) 
{
	uint64_t size, i;
	alloc40_t *alloc;
	aal_device_t *device;
    
	aal_assert("vpf-554", entity != NULL, return -1);
	aal_assert("umka-1746", func != NULL, return -1);

	alloc = (alloc40_t *)entity;
	
	aal_assert("vpf-554", alloc->bitmap != NULL, return -1);
	aal_assert("vpf-554", alloc->device != NULL, return -1);
	
	size = aal_device_get_bs(alloc->device) - CRC_SIZE;
    
	for (i = blk / size; i < blk / size + size; i++) {
		errno_t res;
		
		if ((res = func(entity, i, data)))
			return res;
	}

	return 0;    
}

/* Calls func for each block allocator block */
static errno_t alloc40_layout(object_entity_t *entity,
			      block_func_t func,
			      void *data) 
{
	count_t bpb;
	alloc40_t *alloc;
	blk_t blk, start;
	uint32_t blocksize;
	
	aal_assert("umka-347", entity != NULL, return -1);
	aal_assert("umka-348", func != NULL, return -1);

	alloc = (alloc40_t *)entity;
	blocksize = aal_device_get_bs(alloc->device);
	
	bpb = (blocksize - CRC_SIZE) * 8;
	start = ALLOC40_START / blocksize;
    
	for (blk = start; blk < start + alloc->bitmap->total;
	     blk = (blk / bpb + 1) * bpb) 
	{
		errno_t res;
		
		if ((res = func(entity, blk, data)))
			return res;
	}
    
	return 0;
}

static errno_t callback_fetch_bitmap(object_entity_t *entity, 
				     uint64_t blk, void *data)
{
	aal_block_t *block;
	char *current, *start;
	uint32_t size, chunk, free;
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-1053", entity != NULL, return -1);

	if (!(block = aal_block_open(alloc->device, blk))) {
		aal_exception_error("Can't read bitmap block %llu. %s.", 
				    blk, alloc->device->error);
		return -1;
	}

	start = aux_bitmap_map(alloc->bitmap);
    
	size = aal_block_size(block) - CRC_SIZE;
	current = start + (size * (blk / size / 8));

	free = (start + alloc->bitmap->size) - current;
	chunk = free > size ? size : free;

	aal_memcpy(current, block->data + CRC_SIZE, chunk);

	aal_memcpy(alloc->crc + (blk / size / 8) * CRC_SIZE,
		   block->data, CRC_SIZE);
	
	aal_block_close(block);
	return 0;
    
 error_free_block:
	aal_block_close(block);
	return -1;
}

static object_entity_t *alloc40_open(aal_device_t *device,
				     uint64_t len)
{
	alloc40_t *alloc;
	uint32_t blocksize, crcsize;
    
	aal_assert("umka-364", device != NULL, return NULL);
	aal_assert("umka-1682", len > 0, return NULL);

	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;
    
	blocksize = aal_device_get_bs(device) - CRC_SIZE;
    
	if (!(alloc->bitmap = aux_bitmap_create(len)))
		goto error_free_alloc;
  
	crcsize = ((alloc->bitmap->size + blocksize - 1) / blocksize) * CRC_SIZE;
    
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
    
	alloc->device = device;
	alloc->plugin = &alloc40_plugin;

	if (alloc40_layout((object_entity_t *)alloc,
			   callback_fetch_bitmap, alloc))
	{
		aal_exception_error("Can't load ondisk bitmap.");
		goto error_free_bitmap;
	}

	aux_bitmap_calc_marked(alloc->bitmap);
    
	return (object_entity_t *)alloc;

 error_free_bitmap:
	aux_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
 error:
	return NULL;
}

#ifndef ENABLE_COMPACT

/* 
   Initializes new alloc40 instance, creates bitmap and return new instance to 
   caller (block allocator in libreiser4).
*/
static object_entity_t *alloc40_create(aal_device_t *device,
				       uint64_t len) 
{
	alloc40_t *alloc;
	uint32_t blocksize, crcsize;

	aal_assert("umka-365", device != NULL, return NULL);
	aal_assert("umka-1683", device != NULL, return NULL);
	
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	blocksize = aal_device_get_bs(device) - CRC_SIZE;
    
	if (!(alloc->bitmap = aux_bitmap_create(len)))
		goto error_free_alloc;
  
	crcsize = (alloc->bitmap->size / blocksize) * CRC_SIZE;
    
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
    
	alloc->device = device;
	alloc->plugin = &alloc40_plugin;
    
	return (object_entity_t *)alloc;

 error_free_bitmap:
	aux_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
 error:
	return NULL;
}

static errno_t alloc40_assign(object_entity_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	aux_bitmap_t *bitmap = (aux_bitmap_t *)data;

	aal_assert("vpf-580", alloc != NULL, return -1);
	aal_assert("vpf-579", bitmap != NULL, return -1);
	
	aal_assert("vpf-581", alloc->bitmap->total == bitmap->total && 
		alloc->bitmap->size == bitmap->size, return -1);

	aal_memcpy(alloc->bitmap->map, bitmap->map, bitmap->size);
	alloc->bitmap->marked = bitmap->marked;

	return 0;
}

static errno_t callback_sync_bitmap(object_entity_t *entity, 
				    uint64_t blk, void *data)
{
	aal_block_t *block;
	char *current, *start; 
	uint32_t size, adler, chunk;
    
	alloc40_t *alloc = (alloc40_t *)entity;
	
	aal_assert("umka-1055", alloc != NULL, return -1);

	if (!(block = aal_block_create(alloc->device, blk, 0xff))) {
		aal_exception_error("Can't read bitmap block %llu. %s.", 
				    blk, alloc->device->error);
		return -1;
	}

	start = aux_bitmap_map(alloc->bitmap);
    
	size = aal_block_size(block) - CRC_SIZE;
	current = start + (size * (blk / size / 8));
    
	/* Updating block which is going to be saved */
	chunk = (start + alloc->bitmap->size) - current > (int)size ? 
		(int)size : (int)((start + alloc->bitmap->size) - current);

	aal_memcpy(block->data + CRC_SIZE, current, chunk);

	if (chunk < size) {
		void *fake;

		if (!(fake = aal_calloc(size, 0xff)))
			goto error_free_block;

		aal_memcpy(fake, current, chunk);
		adler = aal_adler32(fake, size);
		
		aal_free(fake);
	} else
		adler = aal_adler32(current, chunk);
	
	aal_memcpy(block->data, &adler, sizeof(adler));
    
	if (aal_block_sync(block)) {
		aal_exception_error("Can't write bitmap block %llu. %s.", 
				    blk, alloc->device->error);
	
		goto error_free_block;
	}

	aal_block_close(block);
    
	return 0;
    
 error_free_block:
	aal_block_close(block);
	return -1;
}

/* Saves alloc40 data (bitmap in fact) to device */
static errno_t alloc40_sync(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-366", alloc != NULL, return -1);
	aal_assert("umka-367", alloc->bitmap != NULL, return -1);
    
	if (alloc40_layout((object_entity_t *)alloc,
			   callback_sync_bitmap, alloc))
	{
		aal_exception_error("Can't save bitmap.");
		return -1;
	}
    
	return 0;
}

#endif

/* Frees alloc40 instance */
static void alloc40_close(object_entity_t *entity) {
    
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-368", alloc != NULL, return);
	aal_assert("umka-369", alloc->bitmap != NULL, return);

	aux_bitmap_close(alloc->bitmap);

	aal_free(alloc->crc);
	aal_free(alloc);
}

#ifndef ENABLE_COMPACT

/* Marks specified block as used in its own bitmap */
static void alloc40_mark(object_entity_t *entity,
			 uint64_t start,
			 uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-370", alloc != NULL, return);
	aal_assert("umka-371", alloc->bitmap != NULL, return);
    
	aux_bitmap_mark_region(alloc->bitmap, start, count);
}

/* Marks "blk" as free */
static errno_t alloc40_release(object_entity_t *entity,
			    uint64_t start, 
			    uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-372", alloc != NULL, return -1);
	aal_assert("umka-373", alloc->bitmap != NULL, return -1);
    
	aux_bitmap_clear_region(alloc->bitmap, start, start + count);
	return 0;
}

/* Finds first free block in bitmap and returns it to caller */
static errno_t alloc40_allocate(object_entity_t *entity,
				uint64_t *start,
				uint64_t *count)
{
	alloc40_t *alloc;
	alloc = (alloc40_t *)entity;
	
	aal_assert("umka-374", alloc != NULL, return -1);
	aal_assert("umka-1771", start != NULL, return -1);
	aal_assert("umka-375", alloc->bitmap != NULL, return -1);
	
	if ((*start = aux_bitmap_find_cleared(alloc->bitmap, 0)) == INVAL_BLK)
		return -1;
    
	aux_bitmap_mark(alloc->bitmap, *start);

	if (count)
		*count = 1;
	
	return 0;
}

static errno_t alloc40_print(object_entity_t *entity,
			     aal_stream_t *stream,
			     uint16_t options)
{
	aal_assert("umka-1467", entity != NULL, return -1);
	aal_assert("umka-1468", stream != NULL, return -1);

	return 0;
}


#endif

/* Returns free blcoks count */
static uint64_t alloc40_free(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-376", alloc != NULL, return INVAL_BLK);
	aal_assert("umka-377", alloc->bitmap != NULL, return INVAL_BLK);
    
	return aux_bitmap_cleared(alloc->bitmap);
}

/* Returns used blocks count */
static uint64_t alloc40_used(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-378", alloc != NULL, return INVAL_BLK);
	aal_assert("umka-379", alloc->bitmap != NULL, return INVAL_BLK);

	return aux_bitmap_marked(alloc->bitmap);
}

/* Checks whether specified blocks are used or not */
static int alloc40_region_used(object_entity_t *entity,
			       uint64_t start, 
			       uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-663", alloc != NULL, return -1);
	aal_assert("umka-664", alloc->bitmap != NULL, return -1);

	return aux_bitmap_test_region_marked(alloc->bitmap, start,
					     start + count);
}

/* Checks whether specified blocks are unused or not */
static int alloc40_region_unused(object_entity_t *entity,
				 uint64_t start, 
				 uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-700", alloc != NULL, return -1);
	aal_assert("vpf-701", alloc->bitmap != NULL, return -1);

	return aux_bitmap_test_region_cleared(alloc->bitmap, start,
					      start + count);
}

static errno_t callback_check_bitmap(object_entity_t *entity, 
				     uint64_t blk, void *data)
{
	char *current, *start;
    
	uint32_t size, n, free;
	uint32_t ladler, cadler, chunk;
	alloc40_t *alloc = (alloc40_t *)entity;
    
	start = aux_bitmap_map(alloc->bitmap);
	size = aal_device_get_bs(alloc->device) - CRC_SIZE;
    
	/* Getting pointer to next bitmap portion */
	n = (blk / size / 8);
	current = start + (n * size);
	    
	/* Getting the checksum from loaded crc map */
	ladler = *((uint32_t *)(alloc->crc + (n * CRC_SIZE)));
	free = (start + alloc->bitmap->size) - current;
    
	/* Calculating adler checksumm for piece of bitmap */
	chunk = free > size ? size : free;

	if (chunk < size) {
		void *fake;

		if (!(fake = aal_calloc(size, 0xff)))
			return -1;

		aal_memcpy(fake, current, chunk);
		cadler = aal_adler32(fake, size);
		
		aal_free(fake);
	} else
		cadler = aal_adler32(current, chunk);

	/* 
	   If loaded checksum and calculated one are not equal, we have
	   corrupted bitmap.
	*/
	if (ladler != cadler) {
		aal_exception_warn("Checksum missmatch in bitmap block %llu. "
				    "Checksum is 0x%x, should be 0x%x.", blk, 
				    ladler, cadler);
	
		return -1;
	}

	return 0;
}

/* Checks allocator on validness using loaded checksums */
errno_t alloc40_valid(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-963", alloc != NULL, return -1);
	aal_assert("umka-964", alloc->bitmap != NULL, return -1);
    
	if (alloc40_layout((object_entity_t *)alloc,
			   callback_check_bitmap, alloc))
		return -1;

	return 0;
}

/* Filling the alloc40 structure by methods */
static reiser4_plugin_t alloc40_plugin = {
	.alloc_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = ALLOC_REISER40_ID,
			.group = 0,
			.type = ALLOC_PLUGIN_TYPE,
			.label = "alloc40",
			.desc = "Space allocator for reiserfs 4.0, ver. " VERSION,
		},
		.open		= alloc40_open,
		.close		= alloc40_close,

#ifndef ENABLE_COMPACT
		.create		= alloc40_create,
		.assign		= alloc40_assign,
		.sync		= alloc40_sync,
		.mark		= alloc40_mark,
		.allocate	= alloc40_allocate,
		.release	= alloc40_release,
		.print		= alloc40_print,
		.region         = alloc40_region,
#else
		.create		= NULL,
		.assign		= NULL,
		.sync		= NULL,
		.mark		= NULL,
		.allocate	= NULL,
		.release	= NULL,
		.print		= NULL,
		.region	        = NULL,
#endif
		.region_used	= alloc40_region_used,
		.region_unused	= alloc40_region_unused,
		.free		= alloc40_free,
		.used		= alloc40_used,
		.valid		= alloc40_valid,
		.layout         = alloc40_layout
	}
};

static reiser4_plugin_t *alloc40_start(reiser4_core_t *c) {
	core = c;
	return &alloc40_plugin;
}

plugin_register(alloc40_start, NULL);
