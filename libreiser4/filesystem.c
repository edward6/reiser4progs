/*
  filesystem.c -- common reiser4 filesystem code.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

/* Enumerates all filesystem areas (block alloc, journal, etc.) */
errno_t reiser4_fs_layout(
	reiser4_fs_t *fs,
	action_func_t action_func, 
	void *data)
{
	if (reiser4_format_skipped(fs->format, action_func, data))
		return -1;
	
	if (reiser4_format_layout(fs->format, action_func, data))
		return -1;
    
	if (reiser4_journal_layout(fs->journal, action_func, data))
		return -1;
    
	return reiser4_alloc_layout(fs->alloc, action_func, data);
}

/* 
   Opens filesysetm on specified host device and journal device. Replays the
   journal if "replay" flag is specified.
*/
reiser4_fs_t *reiser4_fs_open(
	aal_device_t *host_device,	    /* device filesystem will lie on */
	aal_device_t *journal_device)       /* device journal will lie on */
{
	rpid_t pid;
	count_t len;
	uint32_t blocksize;
	
	reiser4_fs_t *fs;

	aal_assert("umka-148", host_device != NULL, return NULL);

	/* Allocating memory and initializing fields */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;

	fs->device = host_device;
	
	/* Reads master super block. See above for details */
	if (!(fs->master = reiser4_master_open(host_device)))
		goto error_free_fs;
    
	if (reiser4_master_valid(fs->master))
		goto error_free_master;

	blocksize = reiser4_master_blocksize(fs->master);
		
	/* Setting actual used block size from master super block */
	if (aal_device_set_bs(host_device, blocksize)) {
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
    
	if ((len = reiser4_format_get_len(fs->format)) == INVAL_BLK)
		goto error_free_format;
    
	/* Initializes block allocator. See alloc.c for details */
	if (!(fs->alloc = reiser4_alloc_open(fs, len)))
		goto error_free_format;

	if (reiser4_alloc_valid(fs->alloc))
		aal_exception_warn("Block allocator data seems corrupted.");
	
	/* Journal device may be not specified. In this case it will not be opened */
	if (journal_device) {
	    
		/* Setting up block size in use for journal device */
		aal_device_set_bs(journal_device, reiser4_fs_blocksize(fs));

		/* Initializing the journal. See  journal.c for details */
		if (!(fs->journal = reiser4_journal_open(fs, journal_device)))
			goto error_free_alloc;
    
		if (reiser4_journal_valid(fs->journal))
			goto error_free_journal;

	}
    
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_journal;
  
	if (reiser4_oid_valid(fs->oid))
		goto error_free_oid;

	/* Opens the tree starting from root block */
	if (!(fs->tree = reiser4_tree_open(fs)))
		goto error_free_oid;
    
	return fs;

 error_free_tree:
	reiser4_tree_close(fs->tree);
 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_journal:
	if (fs->journal)
		reiser4_journal_close(fs->journal);
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

aal_device_t *reiser4_fs_host_device(reiser4_fs_t *fs) {
	aal_assert("umka-970", fs != NULL, return NULL);
	aal_assert("umka-971", fs->format != NULL, return NULL);

	return fs->device;
}

aal_device_t *reiser4_fs_journal_device(reiser4_fs_t *fs) {
	aal_assert("umka-972", fs != NULL, return NULL);

	return (fs->journal ? fs->journal->device : NULL);
}

static errno_t callback_check_block(
	object_entity_t *entity,
	uint64_t blk, void *data)
{
	return -(blk == *(uint64_t *)data);
}

reiser4_belong_t reiser4_fs_belongs(
	reiser4_fs_t *fs,
	blk_t blk)
{
	aal_assert("umka-1534", fs != NULL, return -1);

	if (reiser4_format_skipped(fs->format, callback_check_block, &blk) != 0)
		return RB_SKIPPED;
	
	if (reiser4_format_layout(fs->format, callback_check_block, &blk) != 0)
		return RB_FORMAT;

	if (reiser4_journal_layout(fs->journal, callback_check_block, &blk) != 0)
		return RB_JOURNAL;

	if (reiser4_alloc_layout(fs->alloc, callback_check_block, &blk) != 0)
		return RB_ALLOC;

	return RB_UNKNOWN;
}

#ifndef ENABLE_COMPACT

/* Destroys reiser4 super block */
errno_t reiser4_fs_clobber(aal_device_t *device) {
	blk_t blk;
	aal_block_t *block;
    
	aal_assert("umka-1273", device != NULL, return -1);

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
	return reiser4_alloc_mark(alloc, blk);
}

/* Marks format area as used */
errno_t reiser4_fs_mark(reiser4_fs_t *fs) {
	aal_assert("umka-1139", fs != NULL, return -1);
	aal_assert("umka-1684", fs->alloc != NULL, return -1);
	
	return reiser4_fs_layout(fs, callback_action_mark, fs->alloc);
}

#define REISER4_MIN_SIZE 122

/* Creates filesystem on specified host and journal devices */
reiser4_fs_t *reiser4_fs_create(
	aal_device_t *host_device,	/* device filesystem will be lie on */
	char *uuid, char *label,        /* uuid and label to be used */
	count_t len,		        /* filesystem length in blocks */
	reiser4_profile_t *profile,	/* profile to be used for new filesystem */
	aal_device_t *journal_device,   /* device journal will be lie on */
	void *journal_hint)	        /* journal params (most probably will be used for r3) */
{
	reiser4_fs_t *fs;
	uint32_t blocksize;
	blk_t blk, master_offset;
	blk_t journal_area_start;
	blk_t journal_area_end;
    
	reiser4_file_hint_t root_hint;

	aal_assert("umka-149", host_device != NULL, return NULL);
	aal_assert("umka-150", journal_device != NULL, return NULL);
	aal_assert("vpf-113", profile != NULL, return NULL);

	blocksize = host_device->blocksize;
	
	/* Makes check for validness of specified block size value */
	if (!aal_pow_of_two(blocksize)) {
		aal_exception_error("Invalid block size %u. It must be power of two.", 
				    blocksize);
		return NULL;
	}

	if (len > aal_device_len(host_device)) {
		aal_exception_error(
			"Device %s is too small (%llu) for filesystem %u blocks long.", 
			aal_device_name(host_device), aal_device_len(host_device), len);
		return NULL;
	}
    
	/* Checks whether filesystem size is enough big */
	if (len < REISER4_MIN_SIZE) {
		aal_exception_error("Requested filesytem size (%llu) too small. "
				    "ReiserFS required minimal size %u blocks long.", 
				    len, REISER4_MIN_SIZE);
		return NULL;
	}
    
	/* Allocating memory and initializing fileds */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = host_device;
	
	/* Creates master super block */
	if (!(fs->master = reiser4_master_create(host_device, profile->format, 
						 blocksize, uuid, label)))
		goto error_free_fs;

	/* Creates disk format */
	if (!(fs->format = reiser4_format_create(fs, len, profile->tail,
						 profile->format)))
		goto error_free_master;

	/* Creates block allocator */
	if (!(fs->alloc = reiser4_alloc_create(fs, len)))
		goto error_free_format;

	/* Creates journal on journal device */
	if (!(fs->journal = reiser4_journal_create(fs, journal_device,
						   journal_hint)))
		goto error_free_alloc;
   
	if (reiser4_fs_mark(fs))
		goto error_free_journal;
    
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_create(fs)))
		goto error_free_journal;

	/* Creates tree */
	if (!(fs->tree = reiser4_tree_create(fs, profile)))
		goto error_free_oid;
    
	return fs;

 error_free_tree:
	reiser4_tree_close(fs->tree);
 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_journal:
	reiser4_journal_close(fs->journal);
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
	aal_assert("umka-231", fs != NULL, return -1);
   
	/* Synchronizing the tree */
	if (reiser4_tree_sync(fs->tree))
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

/* 
   Closes all filesystem's entities. Calls plugins' "done" routine for every
   plugin and frees all assosiated memory.
*/
void reiser4_fs_close(
	reiser4_fs_t *fs)		/* filesystem to be closed */
{
    
	aal_assert("umka-230", fs != NULL, return);
    
	/* Closong the all filesystem objects */
	reiser4_tree_close(fs->tree);
	reiser4_oid_close(fs->oid);
    
	if (fs->journal)
		reiser4_journal_close(fs->journal);
	
	reiser4_alloc_close(fs->alloc);
	reiser4_format_close(fs->format);
	reiser4_master_close(fs->master);

	/* Freeing memory occupied by fs instance */
	aal_free(fs);
}

/* Returns format string from disk format object (for instance, reiserfs 4.0) */
const char *reiser4_fs_name(
	reiser4_fs_t *fs)		/* fs format name will be obtained from */
{
	return reiser4_format_name(fs->format);
}

/* Returns disk format plugin in use */
rpid_t reiser4_fs_format_pid(
	reiser4_fs_t *fs)		/* fs disk format pid will be obtained from */
{
	aal_assert("umka-151", fs != NULL, return INVAL_PID);
	aal_assert("umka-152", fs->master != NULL, return INVAL_PID);

	return reiser4_master_format(fs->master);
}

/* Returns filesystem block size value */
uint16_t reiser4_fs_blocksize(
	reiser4_fs_t *fs)		/* fs blocksize will be obtained from */
{
	aal_assert("umka-153", fs != NULL, return 0);
	aal_assert("umka-154", fs->master != NULL, return 0);
    
	return reiser4_master_blocksize(fs->master);
}

