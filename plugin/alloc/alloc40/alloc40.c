/*
  alloc40.c -- Default block allocator plugin for reiser4. It is bitmap based,
  and it deals with bitmap blocks. For the all bitmap-related actions, we use
  aux_bitmap from the libaux.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "alloc40.h"

#define ALLOC40_START (MASTER_OFFSET + (4096 * 2))

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t alloc40_plugin;

extern errno_t alloc40_related_region(object_entity_t *entity, blk_t blk, 
				      block_func_t func, void *data);

/*
  Calls func for each block allocator block. This function is used in all block
  block allocator operations like load, save, etc.
*/
static errno_t alloc40_layout(object_entity_t *entity,
			      block_func_t func,
			      void *data) 
{
	count_t bpb;
	alloc40_t *alloc;
	blk_t blk, start;
	uint32_t blocksize;
	
	aal_assert("umka-347", entity != NULL);
	aal_assert("umka-348", func != NULL);

	alloc = (alloc40_t *)entity;
	blocksize = aal_device_get_bs(alloc->device);

	/*
	  Calculating block-per-bitmap value. I mean the number of blocks one
	  bitmap block describes. It is calulating such maner because we should
	  count also four bytes for checksum at begibnning og the each bitmap
	  block.
	*/
	bpb = (blocksize - CRC_SIZE) * 8;
	start = ALLOC40_START / blocksize;

	/* Loop though the all bitmap blocks */
	for (blk = start; blk < start + alloc->bitmap->total;
	     blk = (blk / bpb + 1) * bpb) 
	{
		errno_t res;
		
		if ((res = func(entity, blk, data)))
			return res;
	}
    
	return 0;
}

/*
  Fetches one bitmap block. Extracts its checksum from teh first 4 bytes and
  saves it in allocator checksums area. Actually this function is callback one
  which is called by alloc40_layout function in order to load all bitmap map
  from the device. See alloc40_open for details.
*/
static errno_t callback_fetch_bitmap(object_entity_t *entity, 
				     uint64_t blk, void *data)
{
	aal_block_t *block;
	char *current, *start;
	uint32_t size, chunk, free;
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-1053", entity != NULL);

	/* Opening block bitmap lies in */
	if (!(block = aal_block_open(alloc->device, blk))) {
		aal_exception_error("Can't read bitmap block %llu. %s.", 
				    blk, alloc->device->error);
		return -1;
	}

	start = aux_bitmap_map(alloc->bitmap);

	/* Calculating bitmap size in bytes its position inside map */
	size = aal_block_size(block) - CRC_SIZE;
	current = start + (size * (blk / size / 8));

	/* Calculating where and how many bytes will be copied */
	free = (start + alloc->bitmap->size) - current;
	chunk = free > size ? size : free;

	/*
	  Copying bitmap data and crc data into corresponding memory areas in
	  block allocator instance.
	*/
	aal_memcpy(current, block->data + CRC_SIZE, chunk);

	aal_memcpy(alloc->crc + (blk / size / 8) * CRC_SIZE,
		   block->data, CRC_SIZE);
	
	aal_block_close(block);
	return 0;
    
 error_free_block:
	aal_block_close(block);
	return -1;
}

/*
  Initializing block allocator instance and loads bitmap into it from the passed
  @device. This functions is implementation of alloc_ops.open plugin method.
*/
static object_entity_t *alloc40_open(aal_device_t *device,
				     uint64_t len)
{
	alloc40_t *alloc;
	uint32_t blocksize, crcsize;
    
	aal_assert("umka-364", device != NULL);
	aal_assert("umka-1682", len > 0);

	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	/*
	  Creating bitmap with passed @len. Value @len is the number of blocks
	  filesystem lies in. In other words it is the filesystem size. This
	  value is the same as partition size sometimes.
	*/
	blocksize = aal_device_get_bs(device) - CRC_SIZE;
    
	if (!(alloc->bitmap = aux_bitmap_create(len)))
		goto error_free_alloc;

	/* Calulating crc array size */
	crcsize = ((alloc->bitmap->size + blocksize - 1) /
		   blocksize) * CRC_SIZE;

	/* Allocating crc array */
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
    
	alloc->device = device;
	alloc->plugin = &alloc40_plugin;

	/*
	  Calling alloc40_layout method with callback_fetch_bitmap callback for
	  loading all the bitmap blocks.
	*/
	if (alloc40_layout((object_entity_t *)alloc, callback_fetch_bitmap, alloc)) {
		aal_exception_error("Can't load ondisk bitmap.");
		goto error_free_bitmap;
	}

	/* Updating bitmap counters (free blocks, etc) */
	aux_bitmap_calc_marked(alloc->bitmap);
    
	return (object_entity_t *)alloc;

 error_free_bitmap:
	aux_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
 error:
	return NULL;
}

#ifndef ENABLE_ALONE

/* 
   Initializes new alloc40 instance, creates bitmap and return new instance to
   caller (block allocator in libreiser4). This function does almost the same as
   alloc40_open. The difference is that it does not load bitmap from the passed
   device.
*/
static object_entity_t *alloc40_create(aal_device_t *device,
				       uint64_t len) 
{
	alloc40_t *alloc;
	uint32_t blocksize, crcsize;

	aal_assert("umka-365", device != NULL);
	aal_assert("umka-1683", device != NULL);
	
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

/* Assignes passed bitmap pointed by @data to block allocator bitmap */
static errno_t alloc40_assign(object_entity_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	aux_bitmap_t *bitmap = (aux_bitmap_t *)data;

	aal_assert("vpf-580", alloc != NULL);
	aal_assert("vpf-579", bitmap != NULL);
	
	aal_assert("vpf-581", alloc->bitmap->total == bitmap->total && 
		alloc->bitmap->size == bitmap->size);

	aal_memcpy(alloc->bitmap->map, bitmap->map, bitmap->size);
	alloc->bitmap->marked = bitmap->marked;

	return 0;
}

/* Callback for saving one bitmap block onto device */
static errno_t callback_sync_bitmap(object_entity_t *entity, 
				    uint64_t blk, void *data)
{
	aal_block_t *block;
	char *current, *start; 
	uint32_t size, adler, chunk;
    
	alloc40_t *alloc = (alloc40_t *)entity;
	
	aal_assert("umka-1055", alloc != NULL);

	/*
	  Allocating new block and filling it by 0xff bytes (all bits are turned
	  on).
	*/
	if (!(block = aal_block_create(alloc->device, blk, 0xff))) {
		aal_exception_error("Can't read bitmap block %llu. %s.", 
				    blk, alloc->device->error);
		return -1;
	}

	start = aux_bitmap_map(alloc->bitmap);
    
	size = aal_block_size(block) - CRC_SIZE;
	current = start + (size * (blk / size / 8));
    
	/* Copying the piece of bitmap map into allocated block to be saved */
	chunk = (start + alloc->bitmap->size) - current > (int)size ? 
		(int)size : (int)((start + alloc->bitmap->size) - current);

	aal_memcpy(block->data + CRC_SIZE, current, chunk);

	/*
	  Calculating adler crc checksum and updating it in block to be
	  saved. For the last block we are calculating it only for significant
	  patr of bitmap.
	*/
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

	/* Saving block onto device it was allocated on */
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

	aal_assert("umka-366", alloc != NULL);
	aal_assert("umka-367", alloc->bitmap != NULL);

	/*
	  Calling "layout" function for saving all bitmap blocks to device
	  block allocator lies on.
	*/
	if (alloc40_layout((object_entity_t *)alloc,
			   callback_sync_bitmap, alloc))
	{
		aal_exception_error("Can't save bitmap.");
		return -1;
	}
    
	return 0;
}

#endif

/* Frees alloc40 instance and all helper structures like bitmap, crcmap, etc */
static void alloc40_close(object_entity_t *entity) {
    
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-368", alloc != NULL);
	aal_assert("umka-369", alloc->bitmap != NULL);

	aux_bitmap_close(alloc->bitmap);

	aal_free(alloc->crc);
	aal_free(alloc);
}

#ifndef ENABLE_ALONE

/* Marks specified region as used in block allocator */
static errno_t alloc40_occupy_region(object_entity_t *entity,
				    uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-370", alloc != NULL);
	aal_assert("umka-371", alloc->bitmap != NULL);
    
	aux_bitmap_mark_region(alloc->bitmap, start, count);
	return 0;
}

/* Marks specified region as free in blockallocator bitmap */
static errno_t alloc40_release_region(object_entity_t *entity,
				      uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-372", alloc != NULL);
	aal_assert("umka-373", alloc->bitmap != NULL);
    
	aux_bitmap_clear_region(alloc->bitmap, start, count);
	return 0;
}

/*
  Tries to find specified @count of free blocks in block allocator. The first
  block of the found free area is stored in @start. Actual found number of
  blocks is retured to caller. This function is mostly needed for handling
  extent allocation.
*/
static uint64_t alloc40_allocate_region(object_entity_t *entity,
					uint64_t *start, uint64_t count)
{
	uint64_t found;
	alloc40_t *alloc;
	
	alloc = (alloc40_t *)entity;
	
	aal_assert("umka-374", alloc != NULL);
	aal_assert("umka-1771", start != NULL);
	aal_assert("umka-375", alloc->bitmap != NULL);

	/* Calling bitmap for gettign free area from it */
	found = aux_bitmap_find_region_cleared(alloc->bitmap,
					       start, count);

	/*
	  Marking found region as occupied if found region has length more then
	  zero. Probably we should implement more flexible behavior here. And
	  probably we should do not mark found blocks as used in hope the caller
	  will decide is found area enough convenient or not. If so, he will
	  call marking found area as occupied by himself.
	*/
	if (found > 0)
		aux_bitmap_mark_region(alloc->bitmap, *start, found);

	return found;
}

/* Handler for "print" method */
static errno_t alloc40_print(object_entity_t *entity,
			     aal_stream_t *stream,
			     uint16_t options)
{
	alloc40_t *alloc;
	
	aal_assert("umka-1778", entity != NULL);
	aal_assert("umka-1779", stream != NULL);

	alloc = (alloc40_t *)entity;

	/*
	  Printing into passed @stream block allocator properties. Here also
	  will be printing of the bitmap bits here later.
	*/
	aal_stream_format(stream, "Block allocator:\n");
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  alloc->plugin->h.label);

	aal_stream_format(stream, "total blocks:\t%llu\n",
			  alloc->bitmap->total);

	aal_stream_format(stream, "used blocks:\t%llu\n",
			  alloc->bitmap->marked);

	aal_stream_format(stream, "free blocks:\t%llu\n",
			  alloc->bitmap->total -
			  alloc->bitmap->marked);
	
	return 0;
}

#endif

/* Returns free blocks count */
static uint64_t alloc40_unused(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-376", alloc != NULL);
	aal_assert("umka-377", alloc->bitmap != NULL);
    
	return aux_bitmap_cleared(alloc->bitmap);
}

/* Returns used blocks count */
static uint64_t alloc40_used(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-378", alloc != NULL);
	aal_assert("umka-379", alloc->bitmap != NULL);

	return aux_bitmap_marked(alloc->bitmap);
}

/* Checks whether specified blocks are used */
static int alloc40_used_region(object_entity_t *entity,
			       uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-663", alloc != NULL);
	aal_assert("umka-664", alloc->bitmap != NULL);

	return aux_bitmap_test_region_marked(alloc->bitmap,
					     start, count);
}

/* Checks whether specified blocks are unused */
static int alloc40_unused_region(object_entity_t *entity,
				 uint64_t start, 
				 uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-700", alloc != NULL);
	aal_assert("vpf-701", alloc->bitmap != NULL);

	return aux_bitmap_test_region_cleared(alloc->bitmap,
					      start, count);
}

/*
  Callback function for checking one bitmap block on validness. Here we just
  calculate actual checksum and compare it with loaded one.
*/
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

/* Checks allocator on validness  */
errno_t alloc40_valid(object_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-963", alloc != NULL);
	aal_assert("umka-964", alloc->bitmap != NULL);

	/*
	  Calling layout function for traversing all the bitmap blocks with
	  checking callback function.
	*/
	if (alloc40_layout((object_entity_t *)alloc,
			   callback_check_bitmap, alloc))
		return -1;

	return 0;
}

/* Prepare alloc40 plugin by menas of filling it with abowe alloc40 methods */
static reiser4_plugin_t alloc40_plugin = {
	.alloc_ops = {
		.h = {
			.handle = empty_handle,
			.id = ALLOC_REISER40_ID,
			.group = 0,
			.type = ALLOC_PLUGIN_TYPE,
			.label = "alloc40",
			.desc = "Space allocator for reiserfs 4.0, ver. " VERSION,
		},
		.open		       = alloc40_open,
		.close		       = alloc40_close,

#ifndef ENABLE_ALONE
		.create		       = alloc40_create,
		.assign		       = alloc40_assign,
		.sync		       = alloc40_sync,
		.print                 = alloc40_print,
		
		.related_region        = alloc40_related_region,
		.occupy_region	       = alloc40_occupy_region,
		.allocate_region       = alloc40_allocate_region,
		.release_region	       = alloc40_release_region,
#else
		.create		       = NULL,
		.assign		       = NULL,
		.sync		       = NULL,
		.print		       = NULL,
		.related_region        = NULL,
		.occupy_region	       = NULL,
		.allocate_region       = NULL,
		.release_region	       = NULL,
#endif
		.used                  = alloc40_used,
		.unused                = alloc40_unused,
		.valid                 = alloc40_valid,
		.layout                = alloc40_layout,
		.used_region           = alloc40_used_region,
		.unused_region         = alloc40_unused_region
	}
};

static reiser4_plugin_t *alloc40_start(reiser4_core_t *c) {
	core = c;
	return &alloc40_plugin;
}

plugin_register(alloc40_start, NULL);
