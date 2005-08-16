/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.c -- filesystem backup methods. */

#ifndef ENABLE_MINIMAL
#include <reiser4/libreiser4.h>

/* Creates the backup of the given @fs. */
reiser4_backup_t *reiser4_backup_create(reiser4_fs_t *fs) {
	reiser4_backup_t *backup;
	uint32_t size;
	
	aal_assert("vpf-1387", fs != NULL);
	
	/* Allocating and initializing the backup. */
	if (!(backup = aal_calloc(sizeof(*backup), 0)))
		return NULL;
	
	backup->fs = fs;

	/* Create the backup block. */
	size = reiser4_master_get_blksize(fs->master);
	if (aal_block_init(&backup->hint.block, 
			   fs->device, size, 0))
	{
		goto error_free_backup;
	}
	
	aal_block_fill(&backup->hint.block, 0);
	
	/* Backup the fs. */
	if (reiser4_fs_backup(fs, &backup->hint))
		goto error_free_block;

	reiser4_backup_mkdirty(backup);
	return backup;
	
error_free_block:
	aal_block_fini(&backup->hint.block);
	
error_free_backup:
	aal_free(backup);
	return NULL;
}

/* Reading backup blocks and comparing their content that must match for 
   all backup blocks. */
static errno_t cb_open_backup(uint64_t start, uint64_t width, void *data) {
	reiser4_backup_t *backup = (reiser4_backup_t *)data;
	aal_block_t *block = (aal_block_t *)backup->data;
	aal_block_t *fblock;
	errno_t res;
	
	fblock = &backup->hint.block;
	
	/* Reading the first backup block. */
	if (fblock->nr == 0) {
		fblock->nr = start;
		return aal_block_read(fblock);
	}

	/* The first backup block has been read already. */
	block->nr = start;
	if ((res = aal_block_read(block)))
		return res;

	if (aal_memcmp(backup->hint.block.data, 
		       block->data, block->size))
	{
		aal_error("Backup block %llu differ from "
			  "previous ones.", start);
		return -EIO;
	}

	return 0;
}

reiser4_backup_t *reiser4_backup_open(reiser4_fs_t *fs) {
	reiser4_backup_t *backup;
	uint32_t size;
	errno_t res;
	
	/* Allocating and initializing the backup. */
	if (!(backup = aal_calloc(sizeof(*backup), 0)))
		return NULL;

	backup->fs = fs;

	/* Create the backup block. */
	size = reiser4_master_get_blksize(fs->master);
	if ((res = aal_block_init(&backup->hint.block, 
				  fs->device, size, 0)))
	{
		goto error_free_backup;
	}
	
	aal_block_fill(&backup->hint.block, 0);

	/* Create the block to compare with the first one. */
	if (!(backup->data = aal_block_alloc(fs->device, size, 0)))
		goto error_fini_backup;

	aal_block_fill(backup->data, 0);
	
	if (reiser4_backup_layout(backup->fs, cb_open_backup, backup))
		goto error_free_block;
	
	aal_block_free(backup->data);
	backup->data = NULL;
	return backup;
	
 error_free_block:
	aal_block_free(backup->data);
 error_fini_backup:
	aal_block_fini(&backup->hint.block);
 error_free_backup:
	aal_free(backup);
	return NULL;
}

/* Check for valideness opened backup. */
errno_t reiser4_backup_valid(reiser4_backup_t *backup) {
	backup_hint_t hint;
	reiser4_fs_t *fs;
	uint32_t size;
	errno_t res;
	
	aal_assert("vpf-1727", backup != NULL);
	
	fs = backup->fs;
	size = reiser4_master_get_blksize(fs->master);
	if ((res = aal_block_init(&hint.block, fs->device, size, 0)))
		return res;
	
	aal_block_fill(&hint.block, 0);

	/* Backup the fs. */
	if ((res = reiser4_fs_backup(fs, &hint)))
		goto error_fini_hint;

	/* Compare the fresh backup with the read one. */
	res = aal_memcmp(backup->hint.block.data, hint.block.data, size);
	
	aal_block_fini(&hint.block);
	return res ? -EIO: 0;
	
 error_fini_hint:
	aal_block_fini(&hint.block);
	return res;
}

/* Frees the fs backup. */
void reiser4_backup_close(reiser4_backup_t *backup) {
	aal_assert("vpf-1398", backup != NULL);
	
	aal_block_fini(&backup->hint.block);	
	aal_free(backup);
}

/* Assign the block to @blk block number and write it. */
static errno_t cb_write_backup(blk_t blk, uint64_t count, void *data) {
	aal_block_t *block = (aal_block_t *)data;

	aal_block_move(block, block->device, blk);
	aal_block_write(block);

	return 0;
}

/* Write the backup to REISER4_BACKUPS_MAX blocks. */
errno_t reiser4_backup_sync(reiser4_backup_t *backup) {
	errno_t res;

	aal_assert("vpf-1410", backup != NULL);
	aal_assert("vpf-1410", backup->fs != NULL);
	
	/* Prepare the block for writing. */
	if (!reiser4_backup_isdirty(backup))
		return 0;

	/* Write the block to all backup copies. */
	res = reiser4_backup_layout(backup->fs, cb_write_backup,
				    &backup->hint.block);

	reiser4_backup_mkclean(backup);
	return res;
}

static errno_t cb_region(uint64_t blk, uint64_t count, void *data) {
	/* 2nd block in the given region is a backup block. */
	*((blk_t *)data) = (count == 1) ? 0 :blk + 1;

	return 0;
}

errno_t reiser4_backup_layout_body(reiser4_alloc_t *alloc, 
				   uint32_t blksize, uint64_t len, 
				   region_func_t func, void *data) 
{
	blk_t copy, prev, blk;
	errno_t res;

	blk = 2;
	prev = 0;

	while (1) {
		blk = BACKUP_EXP_LAYOUT(blk);

		if (blk <= prev)
			continue;

		if (blk > len) 
			return 0;

		reiser4_alloc_region(alloc, blk, cb_region, &copy);

		if (copy < REISER4_BACKUP_START(blksize)) 
			copy = REISER4_BACKUP_START(blksize);

		if (copy > len)
			return 0;

		if (copy <= prev)
			continue;

		if ((res = func(copy, 1, data)))
			return res;

		prev = copy;
	}
}

/* Backup is saved in REISER4_BACKUPS_MAX blocks spreaded across the fs 
   aligned by the next bitmap block.
   
   Note: Backup should not be touched another time -- do not open them 
   another time, even for the layout operation. */
errno_t reiser4_backup_layout(reiser4_fs_t *fs, 
			      region_func_t func, 
			      void *data)
{
	count_t len, blksize;
	
	aal_assert("vpf-1399", fs != NULL);
	aal_assert("vpf-1400", func != NULL);

	len = reiser4_format_get_len(fs->format);
	blksize = reiser4_master_get_blksize(fs->master);
	
	return reiser4_backup_layout_body(fs->alloc, blksize, len, func, data);
}

static errno_t cb_region_last(blk_t blk, uint64_t count, void *data) {
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

		if ((res = region_func(copy, 1, data)))
			return res;

		prev = copy;
	}

	return 0;
}

#endif
