/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   backup.c -- filesystem backup methods. */

#include <repair/librepair.h>

static errno_t callback_pack(void *object, blk_t blk, 
			     uint64_t count, void *data) 
{
	reiser4_fs_t *fs = (reiser4_fs_t *)object;
	aal_stream_t *stream = (aal_stream_t *)data;

	count_t size;
	aal_block_t *block;
	
	/* Open the block to be packed. */
	size = reiser4_master_get_blksize(fs->master);
	
	if (!(block = aal_block_load(fs->device, size, blk))) {
		aal_error("Failed to load a block (%llu) of "
			  "the fs metadata backup.", blk);
		return -EIO;
	}

	aal_stream_write(stream, block->data, block->size);
	aal_block_free(block);

	return 0;
}

errno_t repair_backup_pack(reiser4_fs_t *fs, aal_stream_t *stream) {
	aal_assert("vpf-1411", fs != NULL);
	aal_assert("vpf-1412", stream != NULL);

	return reiser4_backup_layout(fs, callback_pack, stream);
}

static errno_t callback_unpack(void *object, blk_t blk, 
			       uint64_t count, void *data) 
{
	reiser4_fs_t *fs = (reiser4_fs_t *)object;
	aal_stream_t *stream = (aal_stream_t *)data;
	
	count_t size;
	aal_block_t *block;

	/* Allocate the block to be unpacked. */
	size = reiser4_master_get_blksize(fs->master);
	
	if (!(block = aal_block_alloc(fs->device, size, blk))) {
		aal_error("Failed to allocate a block (%llu) "
			  "for the fs metadata backup.", blk);
		return -ENOMEM;
	}

	/* Read from the stream to the allocated block and write the block. */
	if (aal_stream_read(stream, block->data, size) != (int64_t)size)
		goto error_free_block;
	
	aal_block_write(block);
	aal_block_free(block);

	return 0;
	
 error_free_block:
	aal_error("Can't unpack the block (%llu). Stream is over?", blk);
	aal_block_free(block);
	return -EIO;
}

errno_t repair_backup_unpack(reiser4_fs_t *fs, aal_stream_t *stream) {
	aal_assert("vpf-1413", fs != NULL);
	aal_assert("vpf-1414", stream != NULL);

	return reiser4_backup_layout(fs, callback_unpack, stream);
}

