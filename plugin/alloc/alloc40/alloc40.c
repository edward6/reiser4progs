/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40.c -- default block allocator plugin for reiser4. It is bitmap based,
   and it deals with bitmap blocks. For the all bitmap-related actions, we use
   reiser4_bitmap from the libaux. */

#ifndef ENABLE_MINIMAL
#include <aux/aux.h>

#include "alloc40.h"
#include "alloc40_repair.h"

#define ALLOC40_BLOCKNR(blksize) \
        (REISER4_MASTER_BLOCKNR(blksize) + 2)

static uint32_t alloc40_get_state(reiser4_alloc_ent_t *entity) {
	aal_assert("umka-2084", entity != NULL);
	return PLUG_ENT(entity)->state;
}

static void alloc40_set_state(reiser4_alloc_ent_t *entity,
			      uint32_t state)
{
	aal_assert("umka-2085", entity != NULL);
	PLUG_ENT(entity)->state = state;
}

/* Calls func for each block allocator block. This function is used in all block
   block allocator operations like load, save, etc. */
errno_t alloc40_layout(reiser4_alloc_ent_t *entity,
		       region_func_t region_func,
		       void *data) 
{
	count_t bpb;
	errno_t res = 0;
	blk_t blk, start;
	
	aal_assert("umka-347", entity != NULL);
	aal_assert("umka-348", region_func != NULL);

	/* Calculating block-per-bitmap value. I mean the number of blocks one
	   bitmap block describes. It is calulating such maner because we should
	   count also four bytes for checksum at the beginning of each bitmap
	   block. */
	bpb = (PLUG_ENT(entity)->blksize - CRC_SIZE) * 8;
	start = ALLOC40_BLOCKNR(PLUG_ENT(entity)->blksize);

	/* Loop though the all bitmap blocks. */
	for (blk = start; blk < start + PLUG_ENT(entity)->bitmap->total;
	     blk = ((blk / bpb) + 1) * bpb) 
	{
		res |= region_func(blk, 1, data);
		
		if (res && res != -ESTRUCT)
			return res;
	}
    
	return res;
}

/* Fetches one bitmap block. Extracts its checksum from teh first 4 bytes and
   saves it in allocator checksums area. Actually this function is callback one
   which is called by alloc40_layout function in order to load all bitmap map
   from the device. See alloc40_open for details. */
static errno_t cb_fetch_bitmap(blk_t start, count_t width, void *data) {
	errno_t res;
	uint64_t offset;
	alloc40_t *alloc;
	aal_block_t block;
	char *current, *map;
	uint32_t size, chunk, free;
    
	alloc = (alloc40_t *)data;

	if ((res = aal_block_init(&block, alloc->device,
				  alloc->blksize, start)))
	{
		return res;
	}

	if ((res = aal_block_read(&block))) {
		aal_error("Can't read bitmap block %llu. %s.",
			  (unsigned long long)start, alloc->device->error);
		goto error_free_block;
	}

	size = alloc->blksize - CRC_SIZE;
	map = alloc->bitmap->map;

	offset = start / size / 8;
	current = map + (size * offset);

	/* Calculating where and how many bytes will be copied */
	free = (map + alloc->bitmap->size) - current;
	chunk = free > size ? size : free;

	/* Copying bitmap data and crc data into corresponding memory areas in
	   block allocator instance. */
	aal_memcpy(current, block.data + CRC_SIZE, chunk);

	*((uint32_t *)(alloc->crc + (offset * CRC_SIZE))) = 
		LE32_TO_CPU(*(uint32_t *)block.data);

 error_free_block:
	aal_block_fini(&block);
	return res;
}

/* Initializing block allocator instance and loads bitmap into it from the
   passed @device. This functions is implementation of alloc_ops.open() plugin
   method. */
static reiser4_alloc_ent_t *alloc40_open(aal_device_t *device, 
					 uint32_t blksize, 
					 uint64_t blocks) 
{
	alloc40_t *alloc;
	uint32_t crcsize;
	uint32_t mapsize;
    
	aal_assert("umka-1682", blocks > 0);
	aal_assert("umka-364", device != NULL);

	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	/* Creating bitmap with passed @len. Value @len is the number of blocks
	   filesystem lies in. In other words it is the filesystem size. This
	   value is the same as partition size sometimes. */
	mapsize = blksize - CRC_SIZE;
    
	if (!(alloc->bitmap = reiser4_bitmap_create(blocks)))
		goto error_free_alloc;

	/* Initializing alloc instance. */
	alloc->state = 0;
	alloc->plug = &alloc40_plug;
	alloc->device = device;
	alloc->blksize = blksize;

	/* Calulating crc array size */
	crcsize = ((alloc->bitmap->size + mapsize - 1) /
		   mapsize) * CRC_SIZE;

	/* Allocating crc array */
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;

	/* Calling alloc40_layout() method with fetch_bitmap() callback to load
	   all bitmap blocks. */
	if (alloc40_layout((reiser4_alloc_ent_t *)alloc, cb_fetch_bitmap, alloc)) {
		aal_error("Can't load ondisk bitmap.");
		goto error_free_bitmap;
	}

	/* Updating bitmap counters (free blocks, etc) */
	reiser4_bitmap_calc_marked(alloc->bitmap);
	return (reiser4_alloc_ent_t *)alloc;

 error_free_bitmap:
	reiser4_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

/* Initializes new alloc40 instance, creates bitmap and return new instance to
   caller (block allocator in libreiser4). This function does almost the same as
   alloc40_open(). The difference is that it does not load bitmap from the
   passed device. */
static reiser4_alloc_ent_t *alloc40_create(aal_device_t *device, 
					   uint32_t blksize, 
					   uint64_t blocks) 
{
	alloc40_t *alloc;
	uint32_t mapsize;
	uint32_t crcsize;

	aal_assert("umka-365", device != NULL);
	aal_assert("umka-1683", blocks > 0);

	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	mapsize = blksize - CRC_SIZE;
    
	if (!(alloc->bitmap = reiser4_bitmap_create(blocks)))
		goto error_free_alloc;
  
	crcsize = (alloc->bitmap->size / mapsize) * CRC_SIZE;
    
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;

	/* Initializing alloc instance. */
	alloc->plug = &alloc40_plug;
	alloc->device = device;
	alloc->blksize = blksize;
	alloc->state = (1 << ENTITY_DIRTY);
    
	return (reiser4_alloc_ent_t *)alloc;

 error_free_bitmap:
	reiser4_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

/* Assignes passed bitmap pointed by @data to block allocator bitmap */
static errno_t alloc40_assign(reiser4_alloc_ent_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	reiser4_bitmap_t *bitmap = (reiser4_bitmap_t *)data;

	aal_assert("vpf-580", alloc != NULL);
	aal_assert("vpf-579", bitmap != NULL);
	
	aal_assert("vpf-581", alloc->bitmap->total == bitmap->total);
	aal_assert("umka-2087", alloc->bitmap->size == bitmap->size);

	aal_memcpy(alloc->bitmap->map, bitmap->map, bitmap->size);
	
	alloc->bitmap->marked = bitmap->marked;
	alloc->state |= (1 << ENTITY_DIRTY);

	return 0;
}

static errno_t alloc40_extract(reiser4_alloc_ent_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	reiser4_bitmap_t *bitmap = (reiser4_bitmap_t *)data;

	aal_assert("umka-2156", alloc != NULL);
	aal_assert("umka-2157", bitmap != NULL);
	
	aal_assert("umka-2158", alloc->bitmap->total == bitmap->total);
	aal_assert("umka-2159", alloc->bitmap->size == bitmap->size);

	aal_memcpy(bitmap->map, alloc->bitmap->map, bitmap->size);
	bitmap->marked = alloc->bitmap->marked;
	
	return 0;
}

/* Callback for saving one bitmap block onto device */
static errno_t cb_sync_bitmap(blk_t start, count_t width, void *data) {
	errno_t res;
	alloc40_t *alloc;
	aal_block_t block;
	char *current, *map; 
	uint32_t size, adler, chunk;
	
	alloc = (alloc40_t *)data;
	
	/* Allocating new block and filling it by 0xff bytes (all bits are
	   turned on). This is needed in order to make the rest of last block
	   filled by 0xff istead of 0x00 as it might be by default. */
	if ((res = aal_block_init(&block, alloc->device,
				  alloc->blksize, start)))
	{
		return res;
	}

	aal_block_fill(&block, 0xff);

	size = block.size - CRC_SIZE;
	map = alloc->bitmap->map;
	current = map + (size * (start / size / 8));
    
	/* Copying the piece of bitmap map into allocated block to be saved */
	chunk = (map + alloc->bitmap->size) - current > (int)size ? 
		(int)size : (int)((map + alloc->bitmap->size) - current);

	aal_memcpy(block.data + CRC_SIZE, current, chunk);

	/* Calculating adler crc checksum and updating it in the block to be
	   saved. For the last block we are calculating it only for significant
	   patr of bitmap. */
	if (chunk < size) {
		void *fake;

		if (!(fake = aal_calloc(size, 0xff))) {
			res = -ENOMEM;
			goto error_free_block;
		}

		aal_memcpy(fake, current, chunk);
		adler = aux_adler32(0, fake, size);
		
		aal_free(fake);
	} else {
		adler = aux_adler32(0, current, chunk);
	}
	
	*((uint32_t *)block.data) = CPU_TO_LE32(adler);

	/* Saving block onto device it was allocated on */
	if ((res = aal_block_write(&block))) {
		aal_error("Can't write bitmap block %llu. "
			  "%s.", (unsigned long long)start,
			  alloc->device->error);
	}

 error_free_block:
	aal_block_fini(&block);
	return res;
}

/* Saves alloc40 data (bitmap in fact) to device */
static errno_t alloc40_sync(reiser4_alloc_ent_t *entity) {
	errno_t res = 0;

	aal_assert("umka-366", entity != NULL);
	aal_assert("umka-367", PLUG_ENT(entity)->bitmap != NULL);

	/* Calling layout() function for saving all bitmap blocks to device
	   block allocator lies on. */
	if ((res = alloc40_layout(entity, cb_sync_bitmap, entity))) {
		aal_error("Can't save bitmap to device.");
		return res;
	}

	PLUG_ENT(entity)->state &= ~(1 << ENTITY_DIRTY);
	return res;
}

/* Frees alloc40 instance and all helper structures like bitmap, crcmap, etc */
static void alloc40_close(reiser4_alloc_ent_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-368", alloc != NULL);
	aal_assert("umka-369", alloc->bitmap != NULL);

	reiser4_bitmap_close(alloc->bitmap);

	aal_free(alloc->crc);
	aal_free(alloc);
}

/* Marks specified region as used in block allocator */
static errno_t alloc40_occupy(reiser4_alloc_ent_t *entity,
			      uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-370", alloc != NULL);
	aal_assert("umka-371", alloc->bitmap != NULL);
    
	reiser4_bitmap_mark_region(alloc->bitmap,
			       start, count);

	alloc->state |= (1 << ENTITY_DIRTY);
	return 0;
}

/* Marks specified region as free in blockallocator bitmap */
static errno_t alloc40_release(reiser4_alloc_ent_t *entity,
			       uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-372", alloc != NULL);
	aal_assert("umka-373", alloc->bitmap != NULL);
    
	reiser4_bitmap_clear_region(alloc->bitmap,
				start, count);

	alloc->state |= (1 << ENTITY_DIRTY);
	return 0;
}

/* Tries to find specified @count of free blocks in block allocator. The first
   block of the found free area is stored in @start. Actual found number of
   blocks is retured to caller. This function is mostly needed for handling
   extent allocation. */
static uint64_t alloc40_allocate(reiser4_alloc_ent_t *entity,
				 uint64_t *start, uint64_t count)
{
	uint64_t found;
	alloc40_t *alloc;
	
	alloc = (alloc40_t *)entity;
	
	aal_assert("umka-374", alloc != NULL);
	aal_assert("umka-1771", start != NULL);
	aal_assert("umka-375", alloc->bitmap != NULL);

	/* Calling bitmap for gettign free area from it */
	found = reiser4_bitmap_find_region(alloc->bitmap,
				       start, count, 0);

	/* Marking found region as occupied if its length more then zero.
	   Probably we should implement more flexible behavior here. And
	   probably we should do not mark found blocks as used in hope the
	   caller will decide, that found area is not enough convenient for
	   him. If so, he will call marking found area as occupied by hands. */
	if (found > 0) {
		reiser4_bitmap_mark_region(alloc->bitmap,
				       *start, found);
		alloc->state |= (1 << ENTITY_DIRTY);
	}

	return found;
}

/* Returns free blocks count */
static uint64_t alloc40_free(reiser4_alloc_ent_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-376", alloc != NULL);
	aal_assert("umka-377", alloc->bitmap != NULL);
    
	return reiser4_bitmap_cleared(alloc->bitmap);
}

/* Returns used blocks count */
static uint64_t alloc40_used(reiser4_alloc_ent_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-378", alloc != NULL);
	aal_assert("umka-379", alloc->bitmap != NULL);

	return reiser4_bitmap_marked(alloc->bitmap);
}

/* Checks whether specified blocks are used */
int alloc40_occupied(reiser4_alloc_ent_t *entity, uint64_t start, uint64_t count) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-663", alloc != NULL);
	aal_assert("umka-664", alloc->bitmap != NULL);

	return reiser4_bitmap_test_region(alloc->bitmap,
					  start, count, 1);
}

/* Checks whether specified blocks are unused */
static int alloc40_available(reiser4_alloc_ent_t *entity,
			     uint64_t start, 
			     uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-700", alloc != NULL);
	aal_assert("vpf-701", alloc->bitmap != NULL);

	return reiser4_bitmap_test_region(alloc->bitmap,
					  start, count, 0);
}

static void cb_inval_warn(blk_t start, uint32_t ladler, uint32_t cadler) {
	aal_error("Checksum mismatch in bitmap block %llu. Checksum "
		  "is 0x%x, should be 0x%x.",
		  (unsigned long long)start, ladler, cadler);
}

typedef void (*inval_func_t) (blk_t, uint32_t, uint32_t);

/* Callback function for checking one bitmap block on validness. Here we just
   calculate actual checksum and compare it with loaded one. */
errno_t cb_valid_block(blk_t start, count_t width, void *data) {
	inval_func_t inval_func;
	alloc40_t *alloc;
	errno_t res;
	
	uint64_t offset;
	uint32_t ladler;
	uint32_t cadler;
	uint32_t chunk;
	uint32_t size;
	uint32_t free;	
	char *current;
	char *map;
	
	alloc = (alloc40_t *)data;
	inval_func = (inval_func_t)alloc->data;
	
	size = alloc->blksize - CRC_SIZE;
	map = alloc->bitmap->map;
    
	/* Getting pointer to next bitmap portion */
	offset = start / size / 8;
	current = map + (offset * size);
	    
	/* Getting the checksum from loaded crc map */
	ladler = *((uint32_t *)(alloc->crc + (offset * CRC_SIZE)));
	free = (map + alloc->bitmap->size) - current;
    
	/* Calculating adler checksumm for piece of bitmap */
	chunk = free > size ? size : free;

	if (chunk < size) {
		void *fake;

		if (!(fake = aal_calloc(size, 0xff)))
			return -ENOMEM;

		aal_memcpy(fake, current, chunk);
		cadler = aux_adler32(0, fake, size);
		
		aal_free(fake);
	} else
		cadler = aux_adler32(0, current, chunk);

	/* If loaded checksum and calculated one are not equal, we have
	   corrupted bitmap. */
	res = (ladler != cadler) ? -ESTRUCT : 0;
	
	if (res && inval_func)
		inval_func(start, ladler, cadler);

	return res;
}

/* Checks allocator on validness  */
errno_t alloc40_valid(reiser4_alloc_ent_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-963", alloc != NULL);
	aal_assert("umka-964", alloc->bitmap != NULL);

	/* Calling layout function for traversing all the bitmap blocks with
	   checking callback function. */
	alloc->data = cb_inval_warn;
	return alloc40_layout(entity, cb_valid_block, alloc);
}

/* Call @func for all blocks which belong to the same bitmap block as passed
   @blk. It is needed for fsck. In the case it detremined that a block is not
   corresponds to its value in block allocator, it should check all the related
   (neighbour) blocks which are described by one bitmap block (4096 -
   CRC_SIZE).*/
errno_t alloc40_region(reiser4_alloc_ent_t *entity, blk_t blk, 
		       region_func_t region_func, void *data) 
{
	alloc40_t *alloc;
	uint64_t start, size;
    
	aal_assert("vpf-554", entity != NULL);
	aal_assert("umka-1746", region_func != NULL);
    
	alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-710", alloc->bitmap != NULL);
    
	size = (alloc->blksize - CRC_SIZE) * 8;
	start = (blk / size) * size;

	/* The last region is of a smaller size. */
	if (start + size > alloc->bitmap->total) {
		size = alloc->bitmap->total - start;
	}
	
	/* Loop though the all blocks one bitmap block describes and calling
	   passed @region_func for each of them. */   
	return region_func(start, size, data);
}

reiser4_alloc_plug_t alloc40_plug = {
	.p = {
		.id = {ALLOC_REISER40_ID, 0, ALLOC_PLUG_TYPE},
		.label = "alloc40",
		.desc  = "Space allocator plugin.",
	},
	
	.open           = alloc40_open,
	.close          = alloc40_close,

	.create         = alloc40_create,
	.assign         = alloc40_assign,
	.extract        = alloc40_extract,
	.sync           = alloc40_sync,
	.pack           = alloc40_pack,
	.unpack         = alloc40_unpack,
	.print          = alloc40_print,

	.used           = alloc40_used,
	.free           = alloc40_free,
	.valid          = alloc40_valid,
	.layout         = alloc40_layout,
	.occupied       = alloc40_occupied,
	.available      = alloc40_available,
	.set_state      = alloc40_set_state,
	.get_state      = alloc40_get_state,

	.layout_bad	= alloc40_layout_bad,
	.region		= alloc40_region,
	.occupy	        = alloc40_occupy,
	.allocate       = alloc40_allocate,
	.release        = alloc40_release,
	.check_struct   = alloc40_check_struct
};

#endif
