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

#ifndef ENABLE_STAND_ALONE

#include "alloc40.h"

#define ALLOC40_START (MASTER_OFFSET + (4096 * 2))

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t alloc40_plugin;

static int alloc40_isdirty(object_entity_t *entity) {
	aal_assert("umka-2084", entity != NULL);
	return ((alloc40_t *)entity)->dirty;
}

static void alloc40_mkdirty(object_entity_t *entity) {
	aal_assert("umka-2085", entity != NULL);
	((alloc40_t *)entity)->dirty = 1;
}

static void alloc40_mkclean(object_entity_t *entity) {
	aal_assert("umka-2086", entity != NULL);
	((alloc40_t *)entity)->dirty = 0;
}

/*
  Call @func for all blocks which belong to the same bitmap block as passed
  @blk. It is needed for fsck. In the case it detremined that a block is not
  corresponds to its value in block allocator, it should check all the related
  (neighbour) blocks which are described by one bitmap block (4096 - CRC_SIZE).
*/
errno_t alloc40_related(object_entity_t *entity, blk_t blk, 
			region_func_t region_func, void *data) 
{
	uint64_t size;
	alloc40_t *alloc;
	aal_device_t *device;
    
	aal_assert("vpf-554", entity != NULL);
	aal_assert("umka-1746", region_func != NULL);
    
	alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-710", alloc->bitmap != NULL);
	aal_assert("vpf-711", alloc->device != NULL);
    
	size = aal_device_get_bs(alloc->device) - CRC_SIZE;
    
	/*
	  Loop though the all blocks one bitmap block describes and calling
	  passed @region_func for each of them.
	*/   
	return region_func(entity, (blk / size) * size, size, data);
}

/*
  Calls func for each block allocator block. This function is used in all block
  block allocator operations like load, save, etc.
*/
errno_t alloc40_layout(object_entity_t *entity,
		       block_func_t block_func,
		       void *data) 
{
	count_t bpb;
	alloc40_t *alloc;
	blk_t blk, start;
	uint32_t blocksize;
	
	aal_assert("umka-347", entity != NULL);
	aal_assert("umka-348", block_func != NULL);

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
	     blk = ((blk / bpb) + 1) * bpb) 
	{
		errno_t res;
		
		if ((res = block_func(entity, blk, data)))
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
	uint64_t offset;
	aal_block_t *block;
	char *current, *start;
	uint32_t size, chunk, free;
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-1053", entity != NULL);

	/* Opening block bitmap lies in */
	if (!(block = aal_block_open(alloc->device, blk))) {
		aal_exception_error("Can't read bitmap block %llu. %s.", 
				    blk, alloc->device->error);
		return -EIO;
	}

	start = aux_bitmap_map(alloc->bitmap);
	size = aal_block_size(block) - CRC_SIZE;

	offset = blk / size / 8;
	current = start + (size * offset);

	/* Calculating where and how many bytes will be copied */
	free = (start + alloc->bitmap->size) - current;
	chunk = free > size ? size : free;

	/*
	  Copying bitmap data and crc data into corresponding memory areas in
	  block allocator instance.
	*/
	aal_memcpy(current, block->data + CRC_SIZE, chunk);

	aal_memcpy(alloc->crc + (offset * CRC_SIZE),		   
		   block->data, CRC_SIZE);
	
	aal_block_close(block);
	return 0;
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

	alloc->dirty = 0;

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
	if (alloc40_layout((object_entity_t *)alloc,
			   callback_fetch_bitmap, alloc))
	{
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
    
	alloc->dirty = 1;
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
	
	aal_assert("vpf-581", alloc->bitmap->total == bitmap->total);
	aal_assert("umka-2087", alloc->bitmap->size == bitmap->size);

	aal_memcpy(alloc->bitmap->map, bitmap->map,
		   bitmap->size);
	
	alloc->bitmap->marked = bitmap->marked;
	alloc->dirty = 1;

	return 0;
}

static errno_t alloc40_extract(object_entity_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	aux_bitmap_t *bitmap = (aux_bitmap_t *)data;

	aal_assert("umka-2156", alloc != NULL);
	aal_assert("umka-2157", bitmap != NULL);
	
	aal_assert("umka-2158", alloc->bitmap->total == bitmap->total);
	aal_assert("umka-2159", alloc->bitmap->size == bitmap->size);

	aal_memcpy(bitmap->map, alloc->bitmap->map,
		   bitmap->size);
	
	bitmap->marked = alloc->bitmap->marked;
	return 0;
}

/* Callback for saving one bitmap block onto device */
static errno_t callback_sync_bitmap(object_entity_t *entity, 
				    uint64_t blk, void *data)
{
	errno_t res = 0;
	aal_block_t *block;
	char *current, *start; 
	uint32_t size, adler, chunk;
    
	alloc40_t *alloc = (alloc40_t *)entity;
	
	aal_assert("umka-1055", alloc != NULL);

	/*
	  Allocating new block and filling it by 0xff bytes (all bits are turned
	  on). This is needed in order to make the rest of last block filled by
	  0xff istead of 0x00 as it might be by default.
	*/
	if (!(block = aal_block_create(alloc->device, blk, 0xff))) {
		aal_exception_error("Can't allocate bitmap block %llu. %s.", 
				    blk, alloc->device->error);
		return -ENOMEM;
	}

	start = aux_bitmap_map(alloc->bitmap);
    
	size = aal_block_size(block) - CRC_SIZE;
	current = start + (size * (blk / size / 8));
    
	/* Copying the piece of bitmap map into allocated block to be saved */
	chunk = (start + alloc->bitmap->size) - current > (int)size ? 
		(int)size : (int)((start + alloc->bitmap->size) - current);

	aal_memcpy(block->data + CRC_SIZE, current, chunk);

	/*
	  Calculating adler crc checksum and updating it in the block to be
	  saved. For the last block we are calculating it only for significant
	  patr of bitmap.
	*/
	if (chunk < size) {
		void *fake;

		if (!(fake = aal_calloc(size, 0xff))) {
			res = -ENOMEM;
			goto error_free_block;
		}

		aal_memcpy(fake, current, chunk);
		adler = aal_adler32(fake, size);
		
		aal_free(fake);
	} else
		adler = aal_adler32(current, chunk);
	
	aal_memcpy(block->data, &adler, sizeof(adler));

	/* Saving block onto device it was allocated on */
	if (aal_block_sync(block)) {
		aal_exception_error("Can't write bitmap block %llu. "
				    "%s.", blk, alloc->device->error);
		res = -EIO;
		goto error_free_block;
	}

 error_free_block:
	aal_block_close(block);
	return res;
}

/* Saves alloc40 data (bitmap in fact) to device */
static errno_t alloc40_sync(object_entity_t *entity) {
	errno_t res;
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-366", alloc != NULL);
	aal_assert("umka-367", alloc->bitmap != NULL);

	/*
	  Calling "layout" function for saving all bitmap blocks to device
	  block allocator lies on.
	*/
	if ((res = alloc40_layout((object_entity_t *)alloc,
				  callback_sync_bitmap, alloc)))
	{
		aal_exception_error("Can't save bitmap.");
		return res;
	}

	alloc->dirty = 0;
	
	return 0;
}

/* Frees alloc40 instance and all helper structures like bitmap, crcmap, etc */
static void alloc40_close(object_entity_t *entity) {
    
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-368", alloc != NULL);
	aal_assert("umka-369", alloc->bitmap != NULL);

	aux_bitmap_close(alloc->bitmap);

	aal_free(alloc->crc);
	aal_free(alloc);
}

/* Marks specified region as used in block allocator */
static errno_t alloc40_occupy(object_entity_t *entity,
			      uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-370", alloc != NULL);
	aal_assert("umka-371", alloc->bitmap != NULL);
    
	aux_bitmap_mark_region(alloc->bitmap, start, count);
	alloc->dirty = 1;
	
	return 0;
}

/* Marks specified region as free in blockallocator bitmap */
static errno_t alloc40_release(object_entity_t *entity,
			       uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-372", alloc != NULL);
	aal_assert("umka-373", alloc->bitmap != NULL);
    
	aux_bitmap_clear_region(alloc->bitmap, start, count);
	alloc->dirty = 1;
	
	return 0;
}

/*
  Tries to find specified @count of free blocks in block allocator. The first
  block of the found free area is stored in @start. Actual found number of
  blocks is retured to caller. This function is mostly needed for handling
  extent allocation.
*/
static uint64_t alloc40_allocate(object_entity_t *entity,
				 uint64_t *start, uint64_t count)
{
	uint64_t found;
	alloc40_t *alloc;
	
	alloc = (alloc40_t *)entity;
	
	aal_assert("umka-374", alloc != NULL);
	aal_assert("umka-1771", start != NULL);
	aal_assert("umka-375", alloc->bitmap != NULL);

	/* Calling bitmap for gettign free area from it */
	found = aux_bitmap_find_region(alloc->bitmap,
				       start, count, 0);

	/*
	  Marking the found region as occupied if its length more then zero.
	  Probably we should implement more flexible behavior here. And probably
	  we should do not mark found blocks as used in hope the caller will
	  decide is found area is not enough convenient for him. If so, he will
	  call marking found area as occupied by himself.
	*/
	if (found > 0) {
		aux_bitmap_mark_region(alloc->bitmap,
				       *start, found);
		alloc->dirty = 1;
	}

	return found;
}

static errno_t callback_print_bitmap(object_entity_t *entity, 
				     uint64_t blk, void *data)
{
	uint32_t size;
	uint64_t offset;

	alloc40_t *alloc;
	aal_stream_t *stream;
	
	alloc = (alloc40_t *)entity;
	stream = (aal_stream_t *)data;

	size = alloc->device->blocksize - CRC_SIZE;
	
	offset = blk / size / 8;
	
	aal_stream_format(stream, "%*llu [ 0x%lx ]\n", 10, blk,
			  *((uint32_t *)alloc->crc + offset));

	return 0;
}

/* Handler for "print" method */
static errno_t alloc40_print(object_entity_t *entity,
			     aal_stream_t *stream,
			     uint16_t options)
{
	errno_t res;
	uint64_t start;
	uint64_t total;
	uint64_t blocks;
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

	aal_stream_format(stream, "\n%*s CRC\n", 10, "BLK");
	aal_stream_format(stream, "-------------------------\n");

	/* Calling alloc40_layout() in order to print all block checksums */
	if ((res = alloc40_layout((object_entity_t *)alloc,
				  callback_print_bitmap, stream)))
	{
		aal_exception_error("Can't print bitmap.");
		return res;
	}
	
	start = 0;
	total = alloc->bitmap->total;

	aal_stream_format(stream, "\nBlock map:\n");
	
	aal_stream_format(stream, "[ ");

	while (start < total) {
		blocks = aux_bitmap_find_region(alloc->bitmap, &start,
					       total - start, 1);

		if (blocks == 0)
			break;

		aal_stream_format(stream, "%llu-%llu ",
				  start, start + blocks);
		
		start += blocks;
	}
	
	aal_stream_format(stream, "]\n");
	
	return 0;
}

/* Returns free blocks count */
static uint64_t alloc40_free(object_entity_t *entity) {
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
static int alloc40_occupied(object_entity_t *entity,
			    uint64_t start,
			    uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-663", alloc != NULL);
	aal_assert("umka-664", alloc->bitmap != NULL);

	return aux_bitmap_test_region(alloc->bitmap,
				      start, count, 1);
}

/* Checks whether specified blocks are unused */
static int alloc40_available(object_entity_t *entity,
			     uint64_t start, 
			     uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-700", alloc != NULL);
	aal_assert("vpf-701", alloc->bitmap != NULL);

	return aux_bitmap_test_region(alloc->bitmap,
				      start, count, 0);
}

/*
  Callback function for checking one bitmap block on validness. Here we just
  calculate actual checksum and compare it with loaded one.
*/
errno_t callback_check_bitmap(object_entity_t *entity, 
			      uint64_t blk, void *data)
{
	char *current, *start;

	uint64_t offset;
	uint32_t size, free;
	uint32_t ladler, cadler, chunk;
	alloc40_t *alloc = (alloc40_t *)entity;
    
	start = aux_bitmap_map(alloc->bitmap);
	size = aal_device_get_bs(alloc->device) - CRC_SIZE;
    
	/* Getting pointer to next bitmap portion */
	offset = blk / size / 8;
	current = start + (offset * size);
	    
	/* Getting the checksum from loaded crc map */
	ladler = *((uint32_t *)(alloc->crc + (offset * CRC_SIZE)));
	free = (start + alloc->bitmap->size) - current;
    
	/* Calculating adler checksumm for piece of bitmap */
	chunk = free > size ? size : free;

	if (chunk < size) {
		void *fake;

		if (!(fake = aal_calloc(size, 0xff)))
			return -ENOMEM;

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
		aal_exception_warn("Checksum missmatch in bitmap "
				   "block %llu. Checksum is 0x%x, "
				   "should be 0x%x.", blk, ladler,
				   cadler);
	
		return -ESTRUCT;
	}

	/* 
	   FIXME-FITALY: Probably the check that the bitmap bit is set should be
	   here also.
	*/
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
	return alloc40_layout((object_entity_t *)alloc,
			      callback_check_bitmap, alloc);
}

extern errno_t alloc40_check(object_entity_t *entity, uint8_t mode);

/* Prepare alloc40 plugin by menas of filling it with abowe alloc40 methods */
static reiser4_plugin_t alloc40_plugin = {
	.alloc_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = ALLOC_REISER40_ID,
			.group = 0,
			.type = ALLOC_PLUGIN_TYPE,
			.label = "alloc40",
			.desc = "Space allocator for reiser4, ver. " VERSION,
		},
		.open		     = alloc40_open,
		.close		     = alloc40_close,

		.create              = alloc40_create,
		.assign              = alloc40_assign,
		.extract             = alloc40_extract,
		.sync		     = alloc40_sync,
		.isdirty             = alloc40_isdirty,
		.mkdirty             = alloc40_mkdirty,
		.mkclean             = alloc40_mkclean,
		.print               = alloc40_print,
		.check               = alloc40_check,
		
		.used                = alloc40_used,
		.free                = alloc40_free,
		.valid               = alloc40_valid,
		.layout              = alloc40_layout,
		.occupied            = alloc40_occupied,
		.available           = alloc40_available,

		.related             = alloc40_related,
		.occupy	             = alloc40_occupy,
		.allocate            = alloc40_allocate,
		.release             = alloc40_release
	}
};

static reiser4_plugin_t *alloc40_start(reiser4_core_t *c) {
	core = c;
	return &alloc40_plugin;
}

plugin_register(alloc40, alloc40_start, NULL);

#endif
