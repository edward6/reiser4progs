/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   status.c -- filesystem status block functions. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/reiser4.h>

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

	status->dirty = FALSE;
	status->device = device;
	status->blksize = blksize;
	
	/* Reading the block where master super block lies */
	if (!(block = aal_block_load(device, status->blksize,
				     REISER4_STATUS_BLOCK)))
	{
		aal_exception_fatal("Can't read status block.");
		goto error_free_status;
	}

	/* Copying master super block */
	aal_memcpy(STATUS(status), block->data, sizeof(*STATUS(status)));

	aal_block_free(block);
    
	/* Reiser4 master super block is not found on the device. */
	if (aal_strncmp(STATUS(status)->ss_magic, REISER4_STATUS_MAGIC,
			aal_strlen(REISER4_STATUS_MAGIC)) != 0)
	{
		aal_exception_error("Wrong magic is found in the "
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

	status->dirty = TRUE;
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
	offset = REISER4_STATUS_BLOCK;

	if (!(block = aal_block_alloc(status->device,
				      blksize, offset)))
	{
		return -ENOMEM;
	}

	aal_block_fill(block, 0);

	aal_memcpy(block->data, STATUS(status),
		   sizeof(*STATUS(status)));
	
	/* Writing status block to device */
	if ((res = aal_block_write(block))) {
		aal_exception_error("Can't write status block "
				    "at %llu. %s.", block->nr,
				    block->device->error);
		goto error_free_block;
	}

	status->dirty = FALSE;

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

	blk = REISER4_STATUS_BLOCK;
	return region_func(status, blk, 1, data);
}

errno_t reiser4_status_pack(reiser4_status_t *status,
			    aal_stream_t *stream)
{
	uint32_t size;
	
	aal_assert("umka-2612", status != NULL);
	aal_assert("umka-2613", stream != NULL);

	/* Write data size. */
	size = sizeof(status->ent);
	aal_stream_write(stream, &size, sizeof(size));

	/* Write status data to @stream. */
	aal_stream_write(stream, &status->ent, size);

	return 0;
}

reiser4_status_t *reiser4_status_unpack(aal_device_t *device,
					uint32_t blksize,
					aal_stream_t *stream)
{
	uint32_t size;
	reiser4_status_t *status;
    
	aal_assert("umka-981", device != NULL);
	aal_assert("umka-2611", stream != NULL);

	/* Read size and check for validness. */
	aal_stream_read(stream, &size, sizeof(size));

	/* Allocating the memory for status super block struct */
	if (!(status = aal_calloc(sizeof(*status), 0)))
		return NULL;
	
	if (size != sizeof(status->ent)) {
		aal_exception_error("Invalid size %u is "
				    "detected in stream.",
				    size);
		goto error_free_status;
	}

	/* Read status data from @stream. */
	aal_stream_read(stream, &status->ent, size);

	status->dirty = TRUE;
	status->device = device;
	status->blksize = blksize;
	
	return status;
	
 error_free_status:
	aal_free(status);
	return NULL;
}

errno_t reiser4_status_print(reiser4_status_t *status,
			     aal_stream_t *stream)
{
	uint64_t state, extended;
	int i;
	
	aal_assert("umka-2493", status != NULL);
	aal_assert("umka-2494", stream != NULL);

	aal_stream_format(stream, "FS status block (%lu):\n", 
			  REISER4_STATUS_BLOCK);

	state = get_ss_status(STATUS(status));
	extended = get_ss_extended(STATUS(status));

	if (!state) {
		aal_stream_format(stream, "FS marked consistent\n");
		return 0;
	}
	
	if (state & FS_CORRUPTED) {
		aal_stream_format(stream, "FS marked corruped\n");
		state &= ~FS_CORRUPTED;
	}

	if (state & (1 << FS_DAMAGED)) {
		aal_stream_format(stream, "FS marked damaged\n");
		state &= ~FS_DAMAGED;
	}

	if (state & FS_DESTROYED) {
		aal_stream_format(stream, "FS marked destroyed\n");
		state &= ~FS_DESTROYED;
	}

	if (state & FS_IO) {
		aal_stream_format(stream, "FS marked having io "
				  "problems\n");
		state &= ~FS_IO;
	}

	if (state) {
		aal_stream_format(stream, "Some unknown status "
				  "flags found: %0xllx\n", state);
	}

	if (extended) {
		aal_stream_format(stream, "Extended status: %0xllx\n",
				  get_ss_extended(STATUS(status)));
	}
	
	if (*status->ent.ss_message != '\0') {
		aal_stream_format(stream, "Status message:\t%s\n",
				  STATUS(status)->ss_message);
	} 
	
	if (!STATUS(status)->ss_stack[0])
		return 0;
	
	aal_stream_format(stream, "Status backtrace:\n");
	
	for (i = 0; i < SS_STACK_SIZE; i++) {
		if (!ss_stack(STATUS(status), i)) {
			aal_stream_format(stream, "\t%d: 0xllx\n", i, 
					  STATUS(status)->ss_stack[i]);
		}
	}
	
	return 0;
}
#endif
