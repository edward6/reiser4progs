/*
  block.c -- data block functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

/* 
  Allocates one block on specified device. Fills its data field by specified
  char. Marks it as ditry and returns it to caller. This function is widely used
  in libreiser4 for working with disk blocks (node.c, almost all plugins).
*/
aal_block_t *aal_block_create(
	aal_device_t *device,	/* device block will eb allocated on */
	blk_t blk,	        /* block number for allocating */
	char c)			/* char for filling allocated block */
{
	aal_block_t *block;

	aal_assert("umka-443", device != NULL);
    
	if (!(block = (aal_block_t *)aal_calloc(sizeof(*block), 0))) {
		aal_exception_error("Out of memory!");
		return NULL;
	}

	block->device = device;
	    
	if (!(block->data = aal_calloc(aal_device_get_bs(device), c))) {
		aal_exception_error("Out of memory!");
		goto error_free_block;
	}
	
	block->blk = blk;
	aal_block_mkdirty(block);
	
	return block;
	
 error_free_block:
	aal_free(block);
 error:
	return NULL;
}

/*
  Reads one block from specified device. Marks it as clean and returns it to
  caller. For reading is used aal_device_read routine, see above for more
  detailed description.
*/
aal_block_t *aal_block_open(
	aal_device_t *device,	/* device block will be read from */
	blk_t blk)		/* block number for reading */
{
	aal_block_t *block;

	aal_assert("umka-444", device != NULL);

	/* Allocating new block at passed position blk */    
	if (!(block = aal_block_create(device, blk, 0)))
		return NULL;

	/* Reading block data from device */
	if (aal_device_read(device, block->data, blk, 1)) {
		aal_block_close(block);
		return NULL;
	}
    
	/* 
	   Mark block as clean. It means, block will not be realy wrote onto
	   device when aal_block_write method will be called, since block was
	   not changed.
	*/
	aal_block_mkclean(block);
    
	return block;
}

#ifndef ENABLE_STAND_ALONE

/* Makes reread of specified block */
errno_t aal_block_reopen(
	aal_block_t *block, 	/* block to be reread */
	aal_device_t *device,	/* device, new block should be reread from */
	blk_t blk)	        /* block number for rereading */
{
	errno_t res;
	
	aal_assert("umka-631", block != NULL);
	aal_assert("umka-632", device != NULL);

	if ((res = aal_device_read(device, block->data, blk, 1)))
		return res;

	aal_block_relocate(block, blk);
	block->device = device;

	return 0;
}

/* 
  Writes specified block onto device. Device reference, block will be wrote
  onto, stored in block->device field. Marks it as clean and returns error code
  to caller.
*/
errno_t aal_block_sync(
	aal_block_t *block)		/* block for writing */
{
	errno_t error;
	blk_t blk;

	aal_assert("umka-446", block != NULL);

	blk = aal_block_number(block);
    
	if ((error = aal_device_write(block->device, block->data, blk, 1)))
		aal_block_mkclean(block);
    
	return error;
}

/* Sets block number */
void aal_block_relocate(
	aal_block_t *block,		/* block, position will be set to */
	blk_t blk)			/* position for setting up */
{
	aal_assert("umka-450", block != NULL);

	/* Checking for passed block validness */
	if (blk > aal_device_len(block->device)) {
		aal_exception_error("Can't setup block into address out of device.");
		return;
	}
    
	block->blk = blk;
}

#endif

/*  Returns block number of specified block */
blk_t aal_block_number(
	aal_block_t *block)		/* block, position will be obtained from */
{
	aal_assert("umka-448", block != NULL);
	return block->blk;
}

uint32_t aal_block_size(aal_block_t *block) {
	aal_assert("umka-1049", block != NULL);
	return block->device->blocksize;
}

/* Frees block instance and all assosiated memory */
void aal_block_close(
	aal_block_t *block)		/* block to be released */
{
	aal_assert("umka-451", block != NULL);
	
	aal_free(block->data);
	aal_free(block);
}
