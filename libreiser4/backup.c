/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.c -- filesystem backup methods. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/libreiser4.h>

/* Creates the backup of the given @fs. */
reiser4_backup_t *reiser4_backup_create(reiser4_fs_t *fs) {
	reiser4_backup_t *backup;
	errno_t res;
	
	aal_assert("vpf-1387", fs != NULL);
	
	/* Allocating and initializing the backup. */
	if (!(backup = aal_calloc(sizeof(*backup), 0)))
		return NULL;
	
	backup->fs = fs;
	
	/* Create the steam. */
	if (!(backup->stream = aal_stream_create(NULL, &memory_stream)))
		goto error_free_backup;
	
	/* Backup the fs. */
	if ((res = reiser4_fs_backup(fs, backup->stream)))
		goto error_close_stream;

	return backup;
	
error_close_stream:
	aal_stream_close(backup->stream);
	
error_free_backup:
	aal_free(backup);
	
	return NULL;
}

/* Frees the fs backup. */
void reiser4_backup_close(reiser4_backup_t *backup) {
	aal_assert("vpf-1398", backup != NULL);
	
	aal_stream_close(backup->stream);
	aal_free(backup);
}

/* Assign the block to @blk block number and write it. */
static errno_t cb_write_block(void *object, blk_t blk, 
			      uint64_t count, void *data) 
{
	aal_block_t *block = (aal_block_t *)data;

	aal_block_move(block, block->device, blk);
	aal_block_write(block);

	return 0;
}

/* Write the backup to REISER4_BACKUPS_MAX blocks. */
void reiser4_backup_sync(reiser4_backup_t *backup) {
	count_t size;
	aal_block_t *block;
	
	aal_assert("vpf-1410", backup != NULL);
	aal_assert("vpf-1410", backup->fs != NULL);
	aal_assert("vpf-1410", backup->stream != NULL);
	aal_assert("vpf-1410", backup->fs->master != NULL);
	
	/* Prepare the block for writing. */
	size = reiser4_master_get_blksize(backup->fs->master);
	
	if (!(block = aal_block_alloc(backup->fs->device, size, 0))) {
		aal_error("Failed to allocate a block "
			  "for the fs metadata backup.");
		return;
	}
	
	/* Read from the beginning of the stream. */
	aal_stream_reset(backup->stream);
	aal_stream_read(backup->stream, block->data, block->size);
	
	/* Write the block to all backup copies. */
	reiser4_backup_layout(backup->fs, cb_write_block, backup->stream);
	
	aal_block_free(block);
}

static errno_t cb_region(void *object, blk_t blk,
			 uint64_t count, void *data)
{
	/* FIXME: This is hardcoded knowledge that bitmap block lies in the 
	   first block of the region. */
	*((blk_t *)data) = (count == 1) ? 0 :blk + 1;

	return 0;
}

/* Put backup copies into the power of (3/2) block numbers. */
#define BACKUP_EXP_LAYOUT(blk)		((blk * 3) >> 1)

/* Backup is saved in REISER4_BACKUPS_MAX blocks spreaded across the fs 
   aligned by the next bitmap block.
   
   Note: Backup should not be touched another time -- do not open them 
   another time, even for the layout operation. */
errno_t reiser4_backup_layout(reiser4_fs_t *fs, 
			      region_func_t region_func,
			      void *data)
{
	count_t len, blksize;
	blk_t copy, prev, blk;
	errno_t res;
	
	aal_assert("vpf-1399", fs != NULL);
	aal_assert("vpf-1400", region_func != NULL);

	len = reiser4_format_get_len(fs->format);
	blksize = reiser4_master_get_blksize(fs->master);
	prev = 0;
	blk = 2;
	
	while (1) {
		blk = BACKUP_EXP_LAYOUT(blk);

		if (blk <= prev)
			continue;

		if (blk > len) 
			return 0;

		reiser4_alloc_region(fs->alloc, blk, cb_region, &copy);

		if (copy < REISER4_BACKUP_START(blksize)) 
			copy = REISER4_BACKUP_START(blksize);

		if (copy <= prev)
			continue;

		if ((res = region_func(fs, copy, 1, data)))
			return res;

		prev = copy;
	}

	return 0;
}

static errno_t cb_region_last(void *object, blk_t blk, 
			      uint64_t count, void *data) 
{
	*((blk_t *)data) = count == 1 ? 0 :
		blk + count - 1;

	return 0;
}

#define REISER4_BACKUPS_MAX 16

errno_t reiser4_old_backup_layout(reiser4_fs_t *fs, 
				  region_func_t region_func,
				  void *data)
{
	errno_t res;
	count_t len;
	count_t delta;
	blk_t prev = 0;
	blk_t blk, copy;
	
	aal_assert("vpf-1399", fs != NULL);
	aal_assert("vpf-1400", region_func != NULL);

	len = reiser4_format_get_len(fs->format);
	delta = len / (REISER4_BACKUPS_MAX + 1);
	
	for (blk = delta - 1; blk < len; blk += delta) {
		reiser4_alloc_region(fs->alloc, blk, cb_region_last, &copy);

		/* If copy == 0 -- it is not possible to have the last copy 
		   on this fs as the last block is the allocator one. If the 
		   blk number for the copy is the same as the previous one, 
		   skip another copy as fs is pretty small. */
		if (!copy || copy == prev)
			continue;

		if ((res = region_func(fs, copy, 1, data)))
			return res;

		prev = copy;
	}

	return 0;
}

#endif
