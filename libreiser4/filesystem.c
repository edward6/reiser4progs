/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   filesystem.c -- common reiser4 filesystem code. */

#include <reiser4/libreiser4.h>

/* Opens filesystem on specified device */

#ifndef ENABLE_STAND_ALONE
reiser4_fs_t *reiser4_fs_open(aal_device_t *device, bool_t check) {
#else
reiser4_fs_t *reiser4_fs_open(aal_device_t *device) {
#endif
	reiser4_fs_t *fs;

#ifndef ENABLE_STAND_ALONE
	count_t blocks;
	uint32_t blksize;
#endif

	aal_assert("umka-148", device != NULL);

	/* Allocating memory and initializing fields */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;

	fs->device = device;
	
	/* Reads master super block. See above for details */
	if (!(fs->master = reiser4_master_open(device)))
		goto error_free_fs;
    
#ifndef ENABLE_STAND_ALONE
	if (check) {
		if (reiser4_master_valid(fs->master))
			goto error_free_master;
	}

	blksize = reiser4_master_get_blksize(fs->master);

	if (!(fs->status = reiser4_status_open(device, blksize)))
		goto error_free_master;
#endif

	/* Initializes used disk format. See format.c for details */
	if (!(fs->format = reiser4_format_open(fs)))
		goto error_free_status;

#ifndef ENABLE_STAND_ALONE
	if (check) {
		if (reiser4_format_valid(fs->format))
			goto error_free_format;
	}
	
	if ((blocks = reiser4_format_get_len(fs->format)) == INVAL_BLK)
		goto error_free_format;

	/* Initializes block allocator. See alloc.c for details */
	if (!(fs->alloc = reiser4_alloc_open(fs, blocks)))
		goto error_free_format;

	if (check) {
		if (reiser4_alloc_valid(fs->alloc)) {
			aal_error("Block allocator data seems corrupted.");
			goto error_free_alloc;
		}
	}
	
	/* Initializes oid allocator. */
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_alloc;
  
	if (check) {
		if (reiser4_oid_valid(fs->oid))
			goto error_free_oid;
	}
#endif
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_oid;


#ifndef ENABLE_STAND_ALONE
	if (check) {
		if (reiser4_opset_init(fs->tree, check))
			goto error_free_oid;
	}
#else
	if (reiser4_opset_init(fs->tree))
		goto error_free_oid;
#endif
	
	return fs;

 error_free_oid:
#ifndef ENABLE_STAND_ALONE
	reiser4_oid_close(fs->oid);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
#endif
	reiser4_format_close(fs->format);
 error_free_status:
#ifndef ENABLE_STAND_ALONE
	reiser4_status_close(fs->status);
 error_free_master:
#endif
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
	return NULL;
}

/* Close all filesystem's objects */
void reiser4_fs_close(reiser4_fs_t *fs) {
    
	aal_assert("umka-230", fs != NULL);

#ifndef ENABLE_STAND_ALONE
	if (!aal_device_readonly(fs->device))
		reiser4_fs_sync(fs);
	reiser4_tree_fini(fs->tree);
#else
	reiser4_tree_close(fs->tree);
#endif

#ifndef ENABLE_STAND_ALONE
	reiser4_oid_close(fs->oid);
	reiser4_alloc_close(fs->alloc);
#endif

	reiser4_format_close(fs->format);
	reiser4_master_close(fs->master);

#ifndef ENABLE_STAND_ALONE
	reiser4_status_close(fs->status);
#endif
	
	/* Freeing memory occupied by fs instance */
	aal_free(fs);
}

#ifndef ENABLE_STAND_ALONE
static errno_t cb_check_block(void *entity, blk_t start,
			      count_t width, void *data)
{
	blk_t blk = *(blk_t *)data;
	return (blk >= start && blk < start + width);
}

/* Returns passed @blk owner */
reiser4_owner_t reiser4_fs_belongs(
	reiser4_fs_t *fs,
	blk_t blk)
{
	aal_assert("umka-1534", fs != NULL);

	/* Checks if passed @blk is master super block */
	if (reiser4_master_layout(fs->master, cb_check_block, &blk))
		return O_MASTER;
	
	/* Checks if passed @blk belongs to format metadata */
	if (reiser4_format_layout(fs->format, cb_check_block, &blk))
		return O_FORMAT;

	/* Checks if passed @blk belongs to oid allocator metadata */
	if (reiser4_oid_layout(fs->oid, cb_check_block, &blk))
		return O_OID;

	/* Checks if passed @blk belongs to journal metadata if journal
	   opened. */
	if (fs->journal) {
		if (reiser4_journal_layout(fs->journal, cb_check_block, &blk))
			return O_JOURNAL;
	}

	/* Check if @blk is filesystem status block. */
	if (reiser4_status_layout(fs->status, cb_check_block, &blk))
		return O_STATUS;
	
	/* Checks if passed @blk belongs to block allocator data */
	if (reiser4_alloc_layout(fs->alloc, cb_check_block, &blk))
		return O_ALLOC;

	if (reiser4_backup_layout(fs, cb_check_block, &blk))
		return O_BACKUP;
	
	return O_UNKNOWN;
}

/* Enumerates all filesystem areas (block alloc, journal, etc.). This is used
   for marking all blocks belong to all fs components as budy in block allocator
   and in fsck. */
errno_t reiser4_fs_layout(reiser4_fs_t *fs,
			  region_func_t region_func, 
			  void *data)
{
	errno_t res;

	if ((res = reiser4_master_layout(fs->master, region_func, data)))
		return res;

	if ((res = reiser4_oid_layout(fs->oid, region_func, data)))
		return res;
	
	if ((res = reiser4_format_layout(fs->format, region_func, data)))
		return res;

	if (fs->journal) {
		if ((res = reiser4_journal_layout(fs->journal,
						  region_func, data)))
		{
			return res;
		}
	}
	
	if ((res = reiser4_status_layout(fs->status, region_func, data)))
		return res;

	if ((res = reiser4_alloc_layout(fs->alloc, region_func, data)))
		return res;

	return reiser4_backup_layout(fs, region_func, data);
}

static errno_t cb_mark_block(void *entity, blk_t start,
			     count_t width, void *data)
{
	return reiser4_alloc_occupy((reiser4_alloc_t *)data,
				    start, width);
}

errno_t reiser4_fs_check_len(reiser4_fs_t *fs, count_t blocks) {
	uint32_t blksize;
	count_t dev_len;

	aal_assert("vpf-1564", fs != NULL);
	
	blksize = reiser4_master_get_blksize(fs->master);

	dev_len = aal_device_len(fs->device) / 
		(blksize / fs->device->blksize);
	
	if (blocks > dev_len) {
		aal_error("Device %s is too small (%llu) for filesystem %llu "
			  "blocks long.", fs->device->name, dev_len, blocks);
		return -EINVAL;
	}

	if (blocks < REISER4_FS_MIN_SIZE(blksize)) {
		aal_error("Requested filesystem size (%llu) is too small. "
			  "Reiser4 required minimal size %u blocks long.",
			  blocks, REISER4_FS_MIN_SIZE(blksize));
		return -EINVAL;
	}

	return 0;
}

/* Create filesystem on specified host device and with passed params */
reiser4_fs_t *reiser4_fs_create(
	aal_device_t *device,           /* device filesystem will be lie on */
	fs_hint_t *hint)                /* filesystem hint */
{
	reiser4_plug_t *format, *policy, *key;

	count_t free;
	reiser4_fs_t *fs;

	aal_assert("vpf-113", hint != NULL);
	aal_assert("umka-149", device != NULL);

	/* Makes check for validness of specified block size value */
	if (!aal_pow2(hint->blksize)) {
		aal_error("Invalid block size %u. It must "
			  "be power of two.", hint->blksize);
		return NULL;
	}
	
	/* Allocating memory and initializing fileds. */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;
		
	/* Create master super block. */
	format = reiser4_profile_plug(PROF_FORMAT);
		
	if (!(fs->master = reiser4_master_create(device, hint->blksize)))
		goto error_free_fs;
	
	if (reiser4_fs_check_len(fs, hint->blocks))
		goto error_free_master;

	/* Setting up master super block. */
	reiser4_master_set_format(fs->master, format->id.id);
	reiser4_master_set_uuid(fs->master, hint->uuid);
	reiser4_master_set_label(fs->master, hint->label);

	if (!(fs->status = reiser4_status_create(device, hint->blksize)))
		goto error_free_master;

	/* Getting tail policy from default params. */
	policy = reiser4_profile_plug(PROF_POLICY);
	
	/* Creates disk format. */
	if (!(fs->format = reiser4_format_create(fs, format, policy, 
						 hint->blocks)))
	{
		goto error_free_status;
	}

	/* Taking care about key flags in format super block */
	key = reiser4_profile_plug(PROF_KEY);
	
	if (key->id.id == KEY_LARGE_ID) {
		plug_call(fs->format->ent->plug->o.format_ops, set_flags,
			  fs->format->ent, (1 << REISER4_LARGE_KEYS));
	} else {
		plug_call(fs->format->ent->plug->o.format_ops,
			  set_flags, fs->format->ent, 0);
	}

	/* Creates block allocator */
	if (!(fs->alloc = reiser4_alloc_create(fs, hint->blocks)))
		goto error_free_format;

	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_create(fs)))
		goto error_free_alloc;
	
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_oid;
	
	if (!(fs->backup = reiser4_backup_create(fs)))
		goto error_free_tree;
	
	if (reiser4_fs_layout(fs, cb_mark_block, fs->alloc)) {
		aal_error("Can't mark filesystem blocks used.");
		goto error_free_backup;
	}

	free = reiser4_alloc_free(fs->alloc);
	reiser4_format_set_free(fs->format, free);

	return fs;

 error_free_backup:
	reiser4_backup_close(fs->backup);
 error_free_tree:
	reiser4_tree_fini(fs->tree);
 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
	reiser4_format_close(fs->format);
 error_free_status:
	reiser4_status_close(fs->status);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
	return NULL;
}

/* Backup the fs -- save all permanent info about the fs info the memory stream
   to be backed up somewhere on the fs. */
errno_t reiser4_fs_backup(reiser4_fs_t *fs, aal_stream_t *stream) {
	errno_t res;

	aal_assert("vpf-1392", fs != NULL);
	aal_assert("vpf-1393", stream != NULL);

	if ((res = reiser4_master_backup(fs->master, stream)))
		return res;

	return reiser4_format_backup(fs->format, stream);
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
	aal_assert("umka-2484", src_fs != NULL);
	aal_assert("umka-2485", dst_fs != NULL);
	
	return reiser4_tree_copy(src_fs->tree, dst_fs->tree);
}

/* Synchronizes all filesystem objects. */
errno_t reiser4_fs_sync(
	reiser4_fs_t *fs)		/* fs instance to be synchronized */
{
	errno_t res;
	
	aal_assert("umka-231", fs != NULL);
   
	/* Synchronizing the tree */
	if ((res = reiser4_tree_sync(fs->tree)))
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

	return reiser4_status_sync(fs->status);
}

#endif

/* Returns the key of the fake root parent */
errno_t reiser4_fs_root_key(reiser4_fs_t *fs,
			    reiser4_key_t *key)
{
	oid_t locality;
	oid_t objectid;
	
	aal_assert("umka-1949", fs != NULL);
	aal_assert("umka-1950", key != NULL);

	key->plug = fs->tree->ent.tpset[TPSET_KEY];
	
#ifndef ENABLE_STAND_ALONE
	locality = reiser4_oid_root_locality(fs->oid);
	objectid = reiser4_oid_root_objectid(fs->oid);
#else
	locality = REISER4_ROOT_LOCALITY;
	objectid = REISER4_ROOT_OBJECTID;
#endif
	return plug_call(key->plug->o.key_ops, build_generic, key,
			 KEY_STATDATA_TYPE, locality, 0, objectid, 0);
}
