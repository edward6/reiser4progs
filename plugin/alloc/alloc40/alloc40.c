/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40.c -- default block allocator plugin for reiser4. It is bitmap based,
   and it deals with bitmap blocks. For the all bitmap-related actions, we use
   aux_bitmap from the libaux. */

#ifndef ENABLE_STAND_ALONE
#include "alloc40.h"

#define ALLOC40_BLOCKNR(blksize) \
        ((REISER4_MASTER_OFFSET / blksize) + 2)

extern reiser4_plug_t alloc40_plug;

/* Calculates the adler32 checksum for the data pointed by "buff" of the length
   "n". This function was originally taken from zlib, version 1.1.3, July 9th,
   1998.

   Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler

   This software is provided 'as-is', without any express or implied warranty.
   In no event will the authors be held liable for any damages arising from the
   use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
   
   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
   
   3. This notice may not be removed or altered from any source distribution.

   Jean-loup Gailly        Mark Adler
   jloup@gzip.org          madler@alumni.caltech.edu

   The above comment is applyed to the only aal_alder32 function. */

#define ADLER_BASE (65521l)
#define ADLER_NMAX (5552)

unsigned int aal_adler32(char *buff, unsigned int n) {
	int k;
	unsigned char *t = buff;
	unsigned int s1 = 1, s2 = 0;

	while (n > 0) {
		k = n < ADLER_NMAX ? n : ADLER_NMAX;
		n -= k;
	
		while (k--) {
			s1 += *t++; 
			s2 += s1;
		}
	
		s1 %= ADLER_BASE;
		s2 %= ADLER_BASE;
	}
    
	return (s2 << 16) | s1;
}

static int alloc40_isdirty(generic_entity_t *entity) {
	aal_assert("umka-2084", entity != NULL);
	return ((alloc40_t *)entity)->dirty;
}

static void alloc40_mkdirty(generic_entity_t *entity) {
	aal_assert("umka-2085", entity != NULL);
	((alloc40_t *)entity)->dirty = 1;
}

static void alloc40_mkclean(generic_entity_t *entity) {
	aal_assert("umka-2086", entity != NULL);
	((alloc40_t *)entity)->dirty = 0;
}

/* Calls func for each block allocator block. This function is used in all block
   block allocator operations like load, save, etc. */
errno_t alloc40_layout(generic_entity_t *entity,
		       region_func_t region_func,
		       void *data) 
{
	count_t bpb;
	errno_t res = 0;
	alloc40_t *alloc;
	blk_t blk, start;
	
	aal_assert("umka-347", entity != NULL);
	aal_assert("umka-348", region_func != NULL);

	alloc = (alloc40_t *)entity;

	/* Calculating block-per-bitmap value. I mean the number of blocks one
	   bitmap block describes. It is calulating such maner because we should
	   count also four bytes for checksum at begibnning og the each bitmap
	   block. */
	bpb = (alloc->blksize - CRC_SIZE) * 8;
	start = ALLOC40_BLOCKNR(alloc->blksize);

	/* Loop though the all bitmap blocks */
	for (blk = start; blk < start + alloc->bitmap->total;
	     blk = ((blk / bpb) + 1) * bpb) 
	{
		if ((res |= region_func(entity, blk, 1, data)) < 0)
			return res;
	}
    
	return res;
}

/* Fetches one bitmap block. Extracts its checksum from teh first 4 bytes and
   saves it in allocator checksums area. Actually this function is callback one
   which is called by alloc40_layout function in order to load all bitmap map
   from the device. See alloc40_open for details. */
static errno_t callback_fetch_bitmap(void *entity, blk_t start,
				     count_t width, void *data)
{
	errno_t res;
	uint64_t offset;
	alloc40_t *alloc;
	aal_block_t block;
	char *current, *map;
	uint32_t size, chunk, free;
    
	aal_assert("umka-1053", entity != NULL);

	alloc = (alloc40_t *)entity;

	if ((res = aal_block_init(&block, alloc->device,
				  alloc->blksize, start)))
	{
		return res;
	}

	if ((res = aal_block_read(&block))) {
		aal_exception_error("Can't read bitmap block %llu. %s.",
				    start, alloc->device->error);
		goto error_free_block;
	}

	size = alloc->blksize - CRC_SIZE;
	map = aux_bitmap_map(alloc->bitmap);

	offset = start / size / 8;
	current = map + (size * offset);

	/* Calculating where and how many bytes will be copied */
	free = (map + alloc->bitmap->size) - current;
	chunk = free > size ? size : free;

	/* Copying bitmap data and crc data into corresponding memory areas in
	   block allocator instance. */
	aal_memcpy(current, block.data + CRC_SIZE, chunk);

	aal_memcpy(alloc->crc + (offset * CRC_SIZE),		   
		   block.data, CRC_SIZE);

 error_free_block:
	aal_block_fini(&block);
	return res;
}

/* Initializing block allocator instance and loads bitmap into it from the
   passed @device. This functions is implementation of alloc_ops.open plugin
   method. */
static generic_entity_t *alloc40_open(aal_device_t *device,
				     uint64_t len,
				     uint32_t blksize)
{
	alloc40_t *alloc;
	uint32_t crcsize;
	uint32_t mapsize;
    
	aal_assert("umka-364", device != NULL);
	aal_assert("umka-1682", len > 0);

	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	/* Creating bitmap with passed @len. Value @len is the number of blocks
	   filesystem lies in. In other words it is the filesystem size. This
	   value is the same as partition size sometimes. */
	mapsize = blksize - CRC_SIZE;
    
	if (!(alloc->bitmap = aux_bitmap_create(len)))
		goto error_free_alloc;

	alloc->dirty = 0;

	/* Calulating crc array size */
	crcsize = ((alloc->bitmap->size + mapsize - 1) /
		   mapsize) * CRC_SIZE;

	/* Allocating crc array */
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
    
	alloc->device = device;
	alloc->blksize = blksize;
	alloc->plug = &alloc40_plug;

	/* Calling alloc40_layout method with callback_fetch_bitmap callback for
	   loading all the bitmap blocks. */
	if (alloc40_layout((generic_entity_t *)alloc,
			   callback_fetch_bitmap, alloc))
	{
		aal_exception_error("Can't load ondisk bitmap.");
		goto error_free_bitmap;
	}

	/* Updating bitmap counters (free blocks, etc) */
	aux_bitmap_calc_marked(alloc->bitmap);
    
	return (generic_entity_t *)alloc;

 error_free_bitmap:
	aux_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

/* Initializes new alloc40 instance, creates bitmap and return new instance to
   caller (block allocator in libreiser4). This function does almost the same as
   alloc40_open. The difference is that it does not load bitmap from the passed
   device. */
static generic_entity_t *alloc40_create(aal_device_t *device,
				       uint64_t len, uint32_t blksize)
{
	alloc40_t *alloc;
	uint32_t mapsize;
	uint32_t crcsize;

	aal_assert("umka-365", device != NULL);
	aal_assert("umka-1683", device != NULL);
	
	if (!(alloc = aal_calloc(sizeof(*alloc), 0)))
		return NULL;

	mapsize = blksize - CRC_SIZE;
    
	if (!(alloc->bitmap = aux_bitmap_create(len)))
		goto error_free_alloc;
  
	crcsize = (alloc->bitmap->size / mapsize) * CRC_SIZE;
    
	if (!(alloc->crc = aal_calloc(crcsize, 0)))
		goto error_free_bitmap;
    
	alloc->dirty = 1;
	alloc->device = device;
	alloc->blksize = blksize;
	alloc->plug = &alloc40_plug;
    
	return (generic_entity_t *)alloc;

 error_free_bitmap:
	aux_bitmap_close(alloc->bitmap);
 error_free_alloc:
	aal_free(alloc);
	return NULL;
}

/* Assignes passed bitmap pointed by @data to block allocator bitmap */
static errno_t alloc40_assign(generic_entity_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	aux_bitmap_t *bitmap = (aux_bitmap_t *)data;

	aal_assert("vpf-580", alloc != NULL);
	aal_assert("vpf-579", bitmap != NULL);
	
	aal_assert("vpf-581", alloc->bitmap->total == bitmap->total);
	aal_assert("umka-2087", alloc->bitmap->size == bitmap->size);

	aal_memcpy(alloc->bitmap->map, bitmap->map, bitmap->size);
	
	alloc->bitmap->marked = bitmap->marked;
	alloc->dirty = 1;

	return 0;
}

static errno_t alloc40_extract(generic_entity_t *entity, void *data) {
	alloc40_t *alloc = (alloc40_t *)entity;
	aux_bitmap_t *bitmap = (aux_bitmap_t *)data;

	aal_assert("umka-2156", alloc != NULL);
	aal_assert("umka-2157", bitmap != NULL);
	
	aal_assert("umka-2158", alloc->bitmap->total == bitmap->total);
	aal_assert("umka-2159", alloc->bitmap->size == bitmap->size);

	aal_memcpy(bitmap->map, alloc->bitmap->map, bitmap->size);
	bitmap->marked = alloc->bitmap->marked;
	
	return 0;
}

/* Callback for saving one bitmap block onto device */
static errno_t callback_sync_bitmap(void *entity, blk_t start,
				    count_t width, void *data)
{
	errno_t res;
	alloc40_t *alloc;
	aal_block_t block;
	char *current, *map; 
	uint32_t size, adler, chunk;
	
	aal_assert("umka-1055", entity != NULL);

	alloc = (alloc40_t *)entity;
	
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
	map = aux_bitmap_map(alloc->bitmap);
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
		adler = aal_adler32(fake, size);
		
		aal_free(fake);
	} else {
		adler = aal_adler32(current, chunk);
	}
	
	aal_memcpy(block.data, &adler, sizeof(adler));

	/* Saving block onto device it was allocated on */
	if ((res = aal_block_write(&block))) {
		aal_exception_error("Can't write bitmap block %llu. "
				    "%s.", start, alloc->device->error);
	}

 error_free_block:
	aal_block_fini(&block);
	return res;
}

/* Saves alloc40 data (bitmap in fact) to device */
static errno_t alloc40_sync(generic_entity_t *entity) {
	errno_t res = 0;
	alloc40_t *alloc;

	alloc = (alloc40_t *)entity;
	
	aal_assert("umka-366", alloc != NULL);
	aal_assert("umka-367", alloc->bitmap != NULL);

	/* Calling "layout" function for saving all bitmap blocks to device
	   block allocator lies on. */
	if ((res = alloc40_layout((generic_entity_t *)alloc,
				  callback_sync_bitmap, alloc)))
	{
		aal_exception_error("Can't save bitmap.");
		return res;
	}

	alloc->dirty = 0;
	return res;
}

/* Frees alloc40 instance and all helper structures like bitmap, crcmap, etc */
static void alloc40_close(generic_entity_t *entity) {
    
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-368", alloc != NULL);
	aal_assert("umka-369", alloc->bitmap != NULL);

	aux_bitmap_close(alloc->bitmap);

	aal_free(alloc->crc);
	aal_free(alloc);
}

/* Marks specified region as used in block allocator */
static errno_t alloc40_occupy(generic_entity_t *entity,
			      uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-370", alloc != NULL);
	aal_assert("umka-371", alloc->bitmap != NULL);
    
	aux_bitmap_mark_region(alloc->bitmap,
			       start, count);
	alloc->dirty = 1;
	
	return 0;
}

/* Marks specified region as free in blockallocator bitmap */
static errno_t alloc40_release(generic_entity_t *entity,
			       uint64_t start, uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-372", alloc != NULL);
	aal_assert("umka-373", alloc->bitmap != NULL);
    
	aux_bitmap_clear_region(alloc->bitmap,
				start, count);
	alloc->dirty = 1;
	
	return 0;
}

/* Tries to find specified @count of free blocks in block allocator. The first
   block of the found free area is stored in @start. Actual found number of
   blocks is retured to caller. This function is mostly needed for handling
   extent allocation. */
static uint64_t alloc40_allocate(generic_entity_t *entity,
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

	/* Marking found region as occupied if its length more then zero.
	   Probably we should implement more flexible behavior here. And
	   probably we should do not mark found blocks as used in hope the
	   caller will decide, that found area is not enough convenient for
	   him. If so, he will call marking found area as occupied by hands. */
	if (found > 0) {
		aux_bitmap_mark_region(alloc->bitmap,
				       *start, found);
		alloc->dirty = 1;
	}

	return found;
}

static errno_t callback_print_bitmap(void *entity, blk_t start,
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

/* Handler for "print" method */
static errno_t alloc40_print(generic_entity_t *entity,
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
	if ((res = alloc40_layout((generic_entity_t *)alloc,
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
		if (!(blocks = aux_bitmap_find_region(alloc->bitmap, &start,
						      total - start, 1)))
		{
			break;
		}

		aal_stream_format(stream, "%llu-%llu ",
				  start, start + blocks);
		
		start += blocks;
	}
	
	aal_stream_format(stream, "]\n");
	
	return 0;
}

/* Returns free blocks count */
static uint64_t alloc40_free(generic_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-376", alloc != NULL);
	aal_assert("umka-377", alloc->bitmap != NULL);
    
	return aux_bitmap_cleared(alloc->bitmap);
}

/* Returns used blocks count */
static uint64_t alloc40_used(generic_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-378", alloc != NULL);
	aal_assert("umka-379", alloc->bitmap != NULL);

	return aux_bitmap_marked(alloc->bitmap);
}

/* Checks whether specified blocks are used */
int alloc40_occupied(generic_entity_t *entity, uint64_t start, uint64_t count) {
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("umka-663", alloc != NULL);
	aal_assert("umka-664", alloc->bitmap != NULL);

	return aux_bitmap_test_region(alloc->bitmap,
				      start, count, 1);
}

/* Checks whether specified blocks are unused */
static int alloc40_available(generic_entity_t *entity,
			     uint64_t start, 
			     uint64_t count) 
{
	alloc40_t *alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-700", alloc != NULL);
	aal_assert("vpf-701", alloc->bitmap != NULL);

	return aux_bitmap_test_region(alloc->bitmap,
				      start, count, 0);
}

static void callback_inval_warn(blk_t start, uint32_t ladler, uint32_t cadler) {
	aal_exception_warn("Checksum missmatch in bitmap block %llu. Checksum "
			   "is 0x%x, should be 0x%x.", start, ladler, cadler);
}

typedef void (*inval_func_t) (blk_t start, uint32_t ladler, uint32_t cadler);

/* Callback function for checking one bitmap block on validness. Here we just
   calculate actual checksum and compare it with loaded one. */
errno_t callback_valid(void *entity, blk_t start, count_t width, void *data) {
	uint32_t chunk;
	uint64_t offset;
	alloc40_t *alloc;
	uint32_t size, free;
	char *current, *map;
	uint32_t ladler, cadler;
	inval_func_t func = (inval_func_t)data;

	alloc = (alloc40_t *)entity;
	size = alloc->blksize - CRC_SIZE;
	map = aux_bitmap_map(alloc->bitmap);
    
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
		cadler = aal_adler32(fake, size);
		
		aal_free(fake);
	} else
		cadler = aal_adler32(current, chunk);

	/* If loaded checksum and calculated one are not equal, we have
	   corrupted bitmap. */
	if (ladler != cadler && func) {
		func(start, ladler, cadler);
		return ESTRUCT;
	}

	return 0;
}

/* Checks allocator on validness  */
errno_t alloc40_valid(generic_entity_t *entity) {
	alloc40_t *alloc = (alloc40_t *)entity;

	aal_assert("umka-963", alloc != NULL);
	aal_assert("umka-964", alloc->bitmap != NULL);

	/* Calling layout function for traversing all the bitmap blocks with
	   checking callback function. */
	return alloc40_layout((generic_entity_t *)alloc, callback_valid, 
			      callback_inval_warn);
}

extern errno_t alloc40_check_struct(generic_entity_t *entity,
				    uint8_t mode);

extern errno_t alloc40_region(generic_entity_t *entity, blk_t blk, 
			      region_func_t func, void *data);

extern errno_t alloc40_layout_bad(generic_entity_t *entity, 
				  region_func_t func,
				  void *data);

static reiser4_alloc_ops_t alloc40_ops = {
	.open           = alloc40_open,
	.close          = alloc40_close,

	.create         = alloc40_create,
	.assign         = alloc40_assign,
	.extract        = alloc40_extract,
	.sync           = alloc40_sync,
	.isdirty        = alloc40_isdirty,
	.mkdirty        = alloc40_mkdirty,
	.mkclean        = alloc40_mkclean,
	.print          = alloc40_print,

	.used           = alloc40_used,
	.free           = alloc40_free,
	.valid          = alloc40_valid,
	.layout         = alloc40_layout,
	.occupied       = alloc40_occupied,
	.available      = alloc40_available,

	.layout_bad	= alloc40_layout_bad,
	.region		= alloc40_region,
	.occupy	        = alloc40_occupy,
	.allocate       = alloc40_allocate,
	.release        = alloc40_release
};

static reiser4_plug_t alloc40_plug = {
	.cl = CLASS_INIT,
	.id = {ALLOC_REISER40_ID, 0, ALLOC_PLUG_TYPE},
	.label = "alloc40",
	.desc  = "Space allocator for reiser4, ver. " VERSION,
	.o = {
		.alloc_ops = &alloc40_ops
	}
};

static reiser4_plug_t *alloc40_start(reiser4_core_t *c) {
	return &alloc40_plug;
}

plug_register(alloc40, alloc40_start, NULL);
#endif
