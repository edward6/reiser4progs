/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   filesystem.c -- common reiser4 filesystem code. */

#include <reiser4/reiser4.h>

/* Opens filesystem on specified device */
reiser4_fs_t *reiser4_fs_open(aal_device_t *device) {
	rid_t pid;

#ifndef ENABLE_STAND_ALONE
	count_t blocks;
#endif
	reiser4_fs_t *fs;

	aal_assert("umka-148", device != NULL);

	/* Allocating memory and initializing fields */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;

	fs->device = device;
	
	/* Reads master super block. See above for details */
	if (!(fs->master = reiser4_master_open(device)))
		goto error_free_fs;
    
#ifndef ENABLE_STAND_ALONE
	if (reiser4_master_valid(fs->master))
		goto error_free_master;
#endif

	/* Initializes used disk format. See format.c for details */
	if (!(fs->format = reiser4_format_open(fs)))
		goto error_free_master;

#ifndef ENABLE_STAND_ALONE
	if (plug_call(fs->format->entity->plug->o.format_ops,
		      tst_flag, fs->format->entity, 0))
	{
		reiser4_profile_override("key", "key_large");
		reiser4_profile_override("cde", "cde_large");
		reiser4_profile_override("node", "node_large");
	} else {
		reiser4_profile_override("key", "key_short");
		reiser4_profile_override("cde", "cde_short");
		reiser4_profile_override("node", "node_short");
	}
	
	if (reiser4_format_valid(fs->format))
		goto error_free_format;
	
	if ((blocks = reiser4_format_get_len(fs->format)) == INVAL_BLK)
		goto error_free_format;

	/* Initializes block allocator. See alloc.c for details */
	if (!(fs->alloc = reiser4_alloc_open(fs, blocks)))
		goto error_free_format;

	if (reiser4_alloc_valid(fs->alloc)) {
		aal_exception_warn("Block allocator data "
				   "seems corrupted.");
	}
	
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_alloc;
  
	if (reiser4_oid_valid(fs->oid))
		goto error_free_oid;
#endif
	
	return fs;
	
#ifndef ENABLE_STAND_ALONE
 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
	reiser4_format_close(fs->format);
#endif
	
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
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

	/* Closing the all filesystem objects */
	reiser4_oid_close(fs->oid);

	reiser4_alloc_close(fs->alloc);
#endif

	reiser4_format_close(fs->format);
	reiser4_master_close(fs->master);

	/* Freeing memory occupied by fs instance */
	aal_free(fs);
}

#ifndef ENABLE_STAND_ALONE
static errno_t callback_check_block(void *entity, uint64_t blk,
				    void *data)
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

	/* Checks if passed @blk belongs to journal metadata if journal
	   opened. */
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

static errno_t callback_mark_block(void *entity, blk_t blk, void *data) {
	return reiser4_alloc_occupy((reiser4_alloc_t *)data, blk, 1);
}

/* Marks filesystem area as used */
errno_t reiser4_fs_mark(reiser4_fs_t *fs) {
	blk_t blk;
	
	aal_assert("umka-1139", fs != NULL);
	aal_assert("umka-1684", fs->alloc != NULL);

        /* Marking master super block */
	blk = REISER4_MASTER_OFFSET /
		reiser4_master_blksize(fs->master);
	
	reiser4_alloc_occupy(fs->alloc, blk, 1);
	
	return reiser4_fs_layout(fs, callback_mark_block,
				 fs->alloc);
}

#define REISER4_MIN_SIZE 122

/* Create filesystem on specified host device and with passed params */
reiser4_fs_t *reiser4_fs_create(
	aal_device_t *device,           /* device filesystem will be lie on */
	fs_hint_t *hint)                /* filesystem hint */
{
	rid_t policy;
	rid_t format;

	count_t free;
	count_t dev_len;
	reiser4_fs_t *fs;

	aal_assert("vpf-113", hint != NULL);
	aal_assert("umka-149", device != NULL);

	/* Makes check for validness of specified block size value */
	if (!aal_pow2(hint->blksize)) {
		aal_exception_error("Invalid block size %u. It must "
				    "be power of two.", hint->blksize);
		return NULL;
	}

	dev_len = aal_device_len(device) /
		(hint->blksize / device->blksize);
	
	if (hint->blocks > dev_len) {
		aal_exception_error("Device %s is too small (%llu) "
				    "for filesystem %llu blocks long.",
				    device->name, dev_len,
				    hint->blocks);
		return NULL;
	}
    
	/* Checks whether filesystem size is enough big */
	if (hint->blocks < REISER4_MIN_SIZE) {
		aal_exception_error("Requested filesytem size (%llu) "
				    "too small. Reiser4 required minimal "
				    "size %u blocks long.", hint->blocks,
				    REISER4_MIN_SIZE);
		return NULL;
	}
    
	/* Allocating memory and initializing fileds */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;
	
	/* Creates master super block */
	format = reiser4_profile_value("format");
		
	if (!(fs->master = reiser4_master_create(device, format,
						 hint->blksize,
						 hint->uuid,
						 hint->label)))
	{
		goto error_free_fs;
	}

	/* Getting tail policy from the passed profile */
	policy = reiser4_profile_value("policy");
	
	/* Creates disk format */
	if (!(fs->format = reiser4_format_create(fs, hint->blocks,
						 policy, format)))
	{
		goto error_free_master;
	}

	/* Taking care about key flags in format super block */
	if (reiser4_profile_value("key") == KEY_LARGE_ID) {
		plug_call(fs->format->entity->plug->o.format_ops,
			  set_flag, fs->format->entity, 0);

		reiser4_profile_override("cde", "cde_large");
		reiser4_profile_override("node", "node_large");
	} else {
		plug_call(fs->format->entity->plug->o.format_ops,
			  clr_flag, fs->format->entity, 0);

		reiser4_profile_override("cde", "cde_short");
		reiser4_profile_override("node", "node_short");
	}

	/* Creates block allocator */
	if (!(fs->alloc = reiser4_alloc_create(fs, hint->blocks)))
		goto error_free_format;

	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_create(fs)))
		goto error_free_alloc;
	
	if (reiser4_fs_mark(fs)) {
		aal_exception_error("Can't mark filesystem used "
				    "blocks.");
		goto error_free_oid;
	}

	free = reiser4_alloc_free(fs->alloc);
	reiser4_format_set_free(fs->format, free);

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
	return reiser4_tree_resize(fs->tree, blocks);
}

/* Makes copy of @src_fs to @dst_fs */
errno_t reiser4_fs_copy(
	reiser4_fs_t *src_fs,           /* fs to be copied */
	reiser4_fs_t *dst_fs)           /* destination fs */
{
	return reiser4_tree_copy(src_fs->tree, dst_fs->tree);
}

/* Synchronizes all filesystem objects. */
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
errno_t reiser4_fs_root_key(reiser4_fs_t *fs,
			    reiser4_key_t *key)
{
	oid_t locality;
	oid_t objectid;
	
	aal_assert("umka-1949", fs != NULL);
	aal_assert("umka-1950", key != NULL);
	aal_assert("umka-1951", key->plug != NULL);

#ifndef ENABLE_STAND_ALONE
	locality = reiser4_oid_root_locality(fs->oid);
	objectid = reiser4_oid_root_objectid(fs->oid);
#else
	locality = REISER4_ROOT_LOCALITY;
	objectid = REISER4_ROOT_OBJECTID;
#endif
	return reiser4_key_build_gener(key, KEY_STATDATA_TYPE,
				       locality, 0, objectid, 0);
}
#endif
