/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.c -- filesystem status block functions. */

#ifndef ENABLE_MINIMAL
#include <reiser4/libreiser4.h>

void reiser4_status_close(reiser4_status_t *status) {
	aal_assert("umka-2487", status != NULL);
	aal_free(status);
}

reiser4_status_t *reiser4_status_open(aal_device_t *device,
				      uint32_t blksize)
{
	aal_block_t *block;
	reiser4_status_t *status;
    
	aal_assert("umka-2490", device != NULL);
    
	if (!(status = aal_calloc(sizeof(*status), 0)))
		return NULL;

	status->dirty = 0;
	status->device = device;
	status->blksize = blksize;
	
	/* Reading the block where master super block lies */
	if (!(block = aal_block_load(device, status->blksize,
				     REISER4_STATUS_BLOCKNR(blksize))))
	{
		aal_fatal("Can't read status block.");
		goto error_free_status;
	}

	/* Copying master super block */
	aal_memcpy(STATUS(status), block->data, sizeof(*STATUS(status)));

	aal_block_free(block);
    
	/* Reiser4 master super block is not found on the device. */
	if (aal_strncmp(STATUS(status)->ss_magic, REISER4_STATUS_MAGIC,
			aal_strlen(REISER4_STATUS_MAGIC)) != 0)
	{
		aal_error("Wrong magic is found in the "
			  "filesystem status block.");
		
		goto error_free_status;
	}
    
	return status;
    
 error_free_status:
	aal_free(status);
	return NULL;
}

reiser4_status_t *reiser4_status_create(aal_device_t *device,
					uint32_t blksize)
{
	reiser4_status_t *status;

	aal_assert("umka-2489", device != NULL);

	if (!(status = aal_calloc(sizeof(*status), 0)))
		return NULL;

	status->dirty = 1;
	status->device = device;
	status->blksize = blksize;

	aal_strncpy(STATUS(status)->ss_magic, REISER4_STATUS_MAGIC,
		    sizeof(STATUS(status)->ss_magic));

	return status;
}

errno_t reiser4_status_sync(reiser4_status_t *status) {
	errno_t res;
	blk_t offset;
	uint32_t blksize;
	aal_block_t *block;
	
	aal_assert("umka-2488", status != NULL);
    
	if (!status->dirty)
		return 0;
	
	blksize = status->blksize;
	offset = REISER4_STATUS_BLOCKNR(status->blksize);

	if (!(block = aal_block_alloc(status->device, blksize, offset)))
		return -ENOMEM;

	aal_block_fill(block, 0);

	aal_memcpy(block->data, STATUS(status),
		   sizeof(*STATUS(status)));
	
	/* Writing status block to device */
	if ((res = aal_block_write(block))) {
		aal_error("Can't write status block "
			  "at %llu. %s.", block->nr,
			  block->device->error);
		goto error_free_block;
	}

	status->dirty = 0;

 error_free_block:
	aal_block_free(block);
	return res;
}

errno_t reiser4_status_layout(reiser4_status_t *status, 
			      region_func_t region_func,
			      void *data)
{
	uint32_t blk;
	
	aal_assert("umka-2491", status != NULL);
	aal_assert("umka-2492", region_func != NULL);

	blk = REISER4_STATUS_BLOCKNR(status->blksize);
	return region_func(blk, 1, data);
}
#endif
