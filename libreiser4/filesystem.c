/*
  filesystem.c -- common reiser4 filesystem code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

/* 
   Opens filesysetm on specified host device and journal device. Replays the
   journal if "replay" flag is specified.
*/
reiser4_fs_t *reiser4_fs_open(aal_device_t *device,
			      reiser4_profile_t *profile)
{
	rid_t pid;

#ifndef ENABLE_STAND_ALONE
	count_t blocks;
#endif
	reiser4_fs_t *fs;
	uint32_t blocksize;

	aal_assert("umka-148", device != NULL);
	
#ifndef ENABLE_STAND_ALONE
	aal_assert("umka-1866", profile != NULL);
#endif

	/* Allocating memory and initializing fields */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;

	fs->device = device;
	
#ifndef ENABLE_STAND_ALONE
	fs->profile = profile;
#endif
	
	/* Reads master super block. See above for details */
	if (!(fs->master = reiser4_master_open(device)))
		goto error_free_fs;
    
#ifndef ENABLE_STAND_ALONE
	if (reiser4_master_valid(fs->master))
		goto error_free_master;
#endif

	blocksize = reiser4_master_blocksize(fs->master);
		
	/* Setting actual used block size from master super block */
	if (aal_device_set_bs(device, blocksize)) {
		aal_exception_error("Invalid block size detected %u.",
				    blocksize);
		goto error_free_master;
	}
    
	/* Initializes used disk format. See format.c for details */
	if (!(fs->format = reiser4_format_open(fs)))
		goto error_free_master;

#ifndef ENABLE_STAND_ALONE
	if (reiser4_format_valid(fs->format))
		goto error_free_format;
	
	if ((blocks = reiser4_format_get_len(fs->format)) == INVAL_BLK)
		goto error_free_format;

	/* Initializes block allocator. See alloc.c for details */
	if (!(fs->alloc = reiser4_alloc_open(fs, blocks)))
		goto error_free_format;

	if (reiser4_alloc_valid(fs->alloc))
		aal_exception_warn("Block allocator data seems corrupted.");
	
#endif
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_alloc;
  
#ifndef ENABLE_STAND_ALONE
	if (reiser4_oid_valid(fs->oid))
		goto error_free_oid;
#endif
	
	return fs;

 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_alloc:

#ifndef ENABLE_STAND_ALONE
	reiser4_alloc_close(fs->alloc);
#endif
	
 error_free_format:
	reiser4_format_close(fs->format);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
 error:
	return NULL;
}

/* Close all filesystem's objects */
void reiser4_fs_close(
	reiser4_fs_t *fs)		/* filesystem to be closed */
{
    
	aal_assert("umka-230", fs != NULL);

#ifndef ENABLE_STAND_ALONE
	if (!aal_device_readonly(fs->device))
		reiser4_fs_sync(fs);
#endif
	
	/* Closing the all filesystem objects */
	reiser4_oid_close(fs->oid);
	
#ifndef ENABLE_STAND_ALONE
	reiser4_alloc_close(fs->alloc);
#endif

	reiser4_format_close(fs->format);
	reiser4_master_close(fs->master);

	/* Freeing memory occupied by fs instance */
	aal_free(fs);
}

#ifndef ENABLE_STAND_ALONE

static errno_t callback_check_block(
	object_entity_t *entity,
	uint64_t blk, void *data)
{
	return -(blk == *(uint64_t *)data);
}

/* Returns passed @blk owner */
reiser4_owner_t reiser4_fs_belongs(
	reiser4_fs_t *fs,
	blk_t blk)
{
	aal_assert("umka-1534", fs != NULL);

	/* Checks if passed @blk belongs to skipped area */
	if (reiser4_format_skipped(fs->format, callback_check_block, &blk) != 0)
		return O_SKIPPED;
	
	/* Checks if passed @blk belongs to format metadata */
	if (reiser4_format_layout(fs->format, callback_check_block, &blk) != 0)
		return O_FORMAT;

	/* Checks if passed @blk belongs to oid allocator metadata */
	if (reiser4_oid_layout(fs->oid, callback_check_block, &blk) != 0)
		return O_OID;

	/*
	  Checks if passed @blk belongs to journal metadata if journal
	  opened.
	*/
	if (fs->journal) {
		if (reiser4_journal_layout(fs->journal,
					   callback_check_block, &blk) != 0)
			return O_JOURNAL;
	}

	/* Checks if passed @blk belongs to block allocator data */
	if (reiser4_alloc_layout(fs->alloc, callback_check_block, &blk) != 0)
		return O_ALLOC;

	return O_UNKNOWN;
}

/* Enumerates all filesystem areas (block alloc, journal, etc.) */
errno_t reiser4_fs_layout(
	reiser4_fs_t *fs,
	block_func_t block_func, 
	void *data)
{
	errno_t res;

	/* Enumerating skipped area */
	if ((res = reiser4_format_skipped(fs->format, block_func, data)))
		return res;
	
	/* Enumerating oid allocator area */
	if ((res = reiser4_oid_layout(fs->oid, block_func, data)))
		return res;
	
	/* Enumerating format area */
	if ((res = reiser4_format_layout(fs->format, block_func, data)))
		return res;

	/* Enumerating journal area */
	if (fs->journal) {
		if ((res = reiser4_journal_layout(fs->journal, block_func, data)))
			return res;
	}
    
	/* Enumerating block allocator area */
	return reiser4_alloc_layout(fs->alloc, block_func, data);
}

/* Destroys reiser4 master super block */
errno_t reiser4_fs_clobber(aal_device_t *device) {
	blk_t blk;
	aal_block_t *block;
    
	aal_assert("umka-1273", device != NULL);

	blk = (MASTER_OFFSET / device->blocksize);
		
	if (!(block = aal_block_create(device, blk, 0)))
		return -EINVAL;

	if (aal_block_sync(block)) {
		aal_exception_error("Can't write block %llu.",
				    aal_block_number(block));
		return -EINVAL;
	}

	return 0;
}

static errno_t callback_action_mark(
	object_entity_t *entity,	/* device for operating on */ 
	blk_t blk,			/* block number to be marked */
	void *data)			/* pointer to block allocator */
{
	reiser4_alloc_t *alloc = (reiser4_alloc_t *)data;
	return reiser4_alloc_occupy(alloc, blk, 1);
}

/* Marks filesystem area as used */
errno_t reiser4_fs_mark(reiser4_fs_t *fs) {
	aal_assert("umka-1139", fs != NULL);
	aal_assert("umka-1684", fs->alloc != NULL);
	
	return reiser4_fs_layout(fs, callback_action_mark,
				 fs->alloc);
}

#define REISER4_MIN_SIZE 122

/* Create filesystem on specified host device and with passed params */
reiser4_fs_t *reiser4_fs_create(
	aal_device_t *device,           /* device filesystem will be lie on */
	char *uuid, char *label,        /* uuid and label to be used */
	reiser4_profile_t *profile,	/* profile to be used for new filesystem */
	count_t blocks)		        /* filesystem length in blocks */
{
	rid_t policy;
	rid_t format;
	
	reiser4_fs_t *fs;
	uint32_t blocksize;

	aal_assert("umka-149", device != NULL);
	aal_assert("vpf-113", profile != NULL);
	aal_assert("umka-1854", blocks > 0);

	blocksize = device->blocksize;
	
	/* Makes check for validness of specified block size value */
	if (!aal_pow2(blocksize)) {
		aal_exception_error("Invalid block size %u. It must be "
				    "power of two.", blocksize);
		return NULL;
	}

	if (blocks > aal_device_len(device)) {
		aal_exception_error("Device %s is too small (%llu) for "
				    "filesystem %u blocks long.",
				    aal_device_name(device),
				    aal_device_len(device), blocks);
		return NULL;
	}
    
	/* Checks whether filesystem size is enough big */
	if (blocks < REISER4_MIN_SIZE) {
		aal_exception_error("Requested filesytem size (%llu) too "
				    "small. ReiserFS required minimal size "
				    "%u blocks long.", blocks, REISER4_MIN_SIZE);
		return NULL;
	}
    
	/* Allocating memory and initializing fileds */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;
	fs->profile = profile;
	
	/* Creates master super block */
	if ((format = reiser4_profile_value(profile, "format")) == INVAL_PID)
		return NULL;
		
	if (!(fs->master = reiser4_master_create(device, format, blocksize,
						 uuid, label)))
		goto error_free_fs;

	/* Getting tail polity from the passed profile */
	if ((policy = reiser4_profile_value(profile, "policy")) == INVAL_PID)
		goto error_free_master;
	
	/* Creates disk format */
	if (!(fs->format = reiser4_format_create(fs, blocks, policy, format)))
		goto error_free_master;

	/* Creates block allocator */
	if (!(fs->alloc = reiser4_alloc_create(fs, blocks)))
		goto error_free_format;

	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_create(fs)))
		goto error_free_alloc;
	
	if (reiser4_fs_mark(fs)) {
		aal_exception_error("Can't mark filesystem used blocks.");
		goto error_free_oid;
	}

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

/* Resizes passed open @fs by passed @blocks */
errno_t reiser4_fs_resize(
	reiser4_fs_t *fs,               /* fs to be resized */
	count_t blocks)                 /* new fs size */
{
	/* FIXME-UMKA: Not implemented yet! */
	return -EINVAL;
}

/* 
  Synchronizes all filesystem objects to corresponding devices (all filesystem
  objects except journal - to host device and journal - to journal device).
*/
errno_t reiser4_fs_sync(
	reiser4_fs_t *fs)		/* fs instance to be synchronized */
{
	errno_t res;
	aal_assert("umka-231", fs != NULL);
   
	/* Synchronizing the tree */
	if (fs->tree && (res = reiser4_tree_sync(fs->tree)))
		return res;
    
	/* Synchronizing block allocator */
	if ((res = reiser4_alloc_sync(fs->alloc)))
		return res;
    
	/* Synchronizing the object allocator */
	if ((res = reiser4_oid_sync(fs->oid)))
		return res;
    
	if ((res = reiser4_format_sync(fs->format)))
		return res;

	if ((res = reiser4_master_sync(fs->master)))
		return res;

	return 0;
}

/* Returns the key of the fake root parent */
errno_t reiser4_fs_hyper_key(reiser4_fs_t *fs, reiser4_key_t *key) {
	oid_t root_locality;
	oid_t hyper_locality;
	
	aal_assert("umka-1949", fs != NULL);
	aal_assert("umka-1950", key != NULL);
	aal_assert("umka-1951", key->plugin != NULL);
	
	root_locality = reiser4_oid_root_locality(fs->oid);
	hyper_locality = reiser4_oid_hyper_locality(fs->oid);
		
	return reiser4_key_build_generic(key, KEY_STATDATA_TYPE, 
					 hyper_locality,
					 root_locality, 0);
}

#endif
