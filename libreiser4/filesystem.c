/*
  filesystem.c -- common reiser4 filesystem code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

/* Enumerates all filesystem areas (block alloc, journal, etc.) */
errno_t reiser4_fs_layout(
	reiser4_fs_t *fs,
	block_func_t func, 
	void *data)
{
	if (reiser4_format_skipped(fs->format, func, data))
		return -1;
	
	if (reiser4_format_layout(fs->format, func, data))
		return -1;

	if (fs->journal) {
		if (reiser4_journal_layout(fs->journal, func, data))
			return -1;
	}
    
	return reiser4_alloc_layout(fs->alloc, func, data);
}

/* 
   Opens filesysetm on specified host device and journal device. Replays the
   journal if "replay" flag is specified.
*/
reiser4_fs_t *reiser4_fs_open(aal_device_t *device) {
	rpid_t pid;
	count_t blocks;
	uint32_t blocksize;
	
	reiser4_fs_t *fs;

	aal_assert("umka-148", device != NULL);

	/* Allocating memory and initializing fields */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;

	fs->device = device;
	
	/* Reads master super block. See above for details */
	if (!(fs->master = reiser4_master_open(device)))
		goto error_free_fs;
    
	if (reiser4_master_valid(fs->master))
		goto error_free_master;

	blocksize = reiser4_master_blocksize(fs->master);
		
	/* Setting actual used block size from master super block */
	if (aal_device_set_bs(device, blocksize)) {
		aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
				    "Invalid block size detected %u.",
				    blocksize);
		goto error_free_master;
	}
    
	/* Initializes used disk format. See format.c for details */
	if (!(fs->format = reiser4_format_open(fs)))
		goto error_free_master;

	if (reiser4_format_valid(fs->format))
		goto error_free_format;
    
	if ((blocks = reiser4_format_get_len(fs->format)) == INVAL_BLK)
		goto error_free_format;
    
	/* Initializes block allocator. See alloc.c for details */
	if (!(fs->alloc = reiser4_alloc_open(fs, blocks)))
		goto error_free_format;

	if (reiser4_alloc_valid(fs->alloc))
		aal_exception_warn("Block allocator data seems corrupted.");
	
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_alloc;
  
	if (reiser4_oid_valid(fs->oid))
		goto error_free_oid;

	return fs;

 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
	reiser4_format_close(fs->format);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
 error:
	return NULL;
}

static errno_t callback_check_block(
	object_entity_t *entity,
	uint64_t blk, void *data)
{
	return -(blk == *(uint64_t *)data);
}

reiser4_owner_t reiser4_fs_belongs(
	reiser4_fs_t *fs,
	blk_t blk)
{
	aal_assert("umka-1534", fs != NULL);

	if (reiser4_format_skipped(fs->format, callback_check_block, &blk) != 0)
		return O_SKIPPED;
	
	if (reiser4_format_layout(fs->format, callback_check_block, &blk) != 0)
		return O_FORMAT;

	if (fs->journal) {
		if (reiser4_journal_layout(fs->journal,
					   callback_check_block, &blk) != 0)
			return O_JOURNAL;
	}

	if (reiser4_alloc_layout(fs->alloc, callback_check_block, &blk) != 0)
		return O_ALLOC;

	return O_UNKNOWN;
}

#ifndef ENABLE_ALONE

/* Destroys reiser4 master super block */
errno_t reiser4_fs_clobber(aal_device_t *device) {
	blk_t blk;
	aal_block_t *block;
    
	aal_assert("umka-1273", device != NULL);

	blk = (MASTER_OFFSET / device->blocksize);
		
	if (!(block = aal_block_create(device, blk, 0)))
		return -1;

	if (aal_block_sync(block)) {
		aal_exception_error("Can't write block %llu.",
				    aal_block_number(block));
		return -1;
	}

	return 0;
}

static errno_t callback_action_mark(
	object_entity_t *entity,	/* device for operating on */ 
	blk_t blk,			/* block number to be marked */
	void *data)			/* pointer to block allocator */
{
	reiser4_alloc_t *alloc = (reiser4_alloc_t *)data;
	return reiser4_alloc_occupy_region(alloc, blk, 1);
}

/* Marks format area as used */
errno_t reiser4_fs_mark(reiser4_fs_t *fs) {
	aal_assert("umka-1139", fs != NULL);
	aal_assert("umka-1684", fs->alloc != NULL);
	
	return reiser4_fs_layout(fs, callback_action_mark, fs->alloc);
}

#define REISER4_MIN_SIZE 122

/* Create filesystem on specified host device and with passed params */
reiser4_fs_t *reiser4_fs_create(
	aal_device_t *device,           /* device filesystem will be lie on */
	char *uuid, char *label,        /* uuid and label to be used */
	reiser4_profile_t *profile,	/* profile to be used for new filesystem */
	count_t blocks)		        /* filesystem length in blocks */
{
	reiser4_fs_t *fs;
	uint32_t blocksize;
	blk_t blk, master_offset;
	blk_t journal_area_start;
	blk_t journal_area_end;
    
	reiser4_file_hint_t root_hint;

	aal_assert("umka-149", device != NULL);
	aal_assert("vpf-113", profile != NULL);
	aal_assert("umka-1854", blocks > 0);

	blocksize = device->blocksize;
	
	/* Makes check for validness of specified block size value */
	if (!aal_pow_of_two(blocksize)) {
		aal_exception_error("Invalid block size %u. It must be "
				    "power of two.", blocksize);
		return NULL;
	}

	if (blocks > aal_device_len(device)) {
		aal_exception_error(
			"Device %s is too small (%llu) for filesystem %u "
			"blocks long.", aal_device_name(device),
			aal_device_len(device), blocks);
		return NULL;
	}
    
	/* Checks whether filesystem size is enough big */
	if (blocks < REISER4_MIN_SIZE) {
		aal_exception_error("Requested filesytem size (%llu) too small. "
				    "ReiserFS required minimal size %u blocks long.", 
				    blocks, REISER4_MIN_SIZE);
		return NULL;
	}
    
	/* Allocating memory and initializing fileds */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;
	
	/* Creates master super block */
	if (!(fs->master = reiser4_master_create(device, profile->format, 
						 blocksize, uuid, label)))
		goto error_free_fs;

	/* Creates disk format */
	if (!(fs->format = reiser4_format_create(fs, blocks, profile->tail,
						 profile->format)))
		goto error_free_master;

	/* Creates block allocator */
	if (!(fs->alloc = reiser4_alloc_create(fs, blocks)))
		goto error_free_format;

	if (reiser4_fs_mark(fs))
		goto error_free_alloc;
    
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_create(fs)))
		goto error_free_alloc;

	return fs;

 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
	reiser4_format_close(fs->format);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
 error:
	return NULL;
}

/* 
  Synchronizes all filesystem objects to corresponding devices (all filesystem
  objects except journal - to host device and journal - to journal device).
*/
errno_t reiser4_fs_sync(
	reiser4_fs_t *fs)		/* fs instance to be synchronized */
{
	aal_assert("umka-231", fs != NULL);
   
	/* Synchronizing the tree */
	if (fs->tree && reiser4_tree_sync(fs->tree))
		return -1;
    
	/* Synchronizing the journal */
	if (fs->journal && reiser4_journal_sync(fs->journal))
		return -1;
    
	/* Synchronizing block allocator */
	if (reiser4_alloc_sync(fs->alloc))
		return -1;
    
	/* Synchronizing the object allocator */
	if (reiser4_oid_sync(fs->oid))
		return -1;
    
	if (reiser4_format_sync(fs->format))
		return -1;

	if (reiser4_master_sync(fs->master))
		return -1;

	return 0;
}

#endif

/* Close all filesystem's objects */
void reiser4_fs_close(
	reiser4_fs_t *fs)		/* filesystem to be closed */
{
    
	aal_assert("umka-230", fs != NULL);
    
	/* Closing the all filesystem objects */
	reiser4_oid_close(fs->oid);
    
	reiser4_alloc_close(fs->alloc);
	reiser4_format_close(fs->format);
	reiser4_master_close(fs->master);

	/* Freeing memory occupied by fs instance */
	aal_free(fs);
}
