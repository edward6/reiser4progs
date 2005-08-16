/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   filesystem.c -- common reiser4 filesystem code. */

#include <reiser4/libreiser4.h>

/* Opens filesystem on specified device */

#ifndef ENABLE_MINIMAL
reiser4_fs_t *reiser4_fs_open(aal_device_t *device, bool_t check) {
#else
reiser4_fs_t *reiser4_fs_open(aal_device_t *device) {
#endif
	reiser4_fs_t *fs;

#ifndef ENABLE_MINIMAL
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
    
#ifndef ENABLE_MINIMAL
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

#ifndef ENABLE_MINIMAL
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


#ifndef ENABLE_MINIMAL
	if (check) {
		if (!(fs->backup = reiser4_backup_open(fs))) {
			aal_error("Failed to open fs backup.");
			goto error_free_tree;
		}
		
		if (reiser4_backup_valid(fs->backup)) {
			aal_error("Reiser4 backup is not consistent.");
			goto error_free_backup;
		}
		
		if (reiser4_opset_init(fs->tree, check))
			goto error_free_backup;
	}
#else
	if (reiser4_opset_init(fs->tree))
		goto error_free_tree;
#endif
	
	return fs;

#ifndef ENABLE_MINIMAL
 error_free_backup:
	reiser4_backup_close(fs->backup);
#endif
 error_free_tree:
	reiser4_tree_close(fs->tree);
 error_free_oid:
#ifndef ENABLE_MINIMAL
	reiser4_oid_close(fs->oid);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
#endif
	reiser4_format_close(fs->format);
 error_free_status:
#ifndef ENABLE_MINIMAL
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

#ifndef ENABLE_MINIMAL
	if (!aal_device_readonly(fs->device))
		reiser4_fs_sync(fs);
#endif

	reiser4_tree_close(fs->tree);

#ifndef ENABLE_MINIMAL
	if (fs->journal)
		reiser4_journal_close(fs->journal);

	reiser4_oid_close(fs->oid);
	reiser4_alloc_close(fs->alloc);
#endif

	reiser4_format_close(fs->format);
	reiser4_master_close(fs->master);

#ifndef ENABLE_MINIMAL
	reiser4_status_close(fs->status);
	
	if (fs->backup) {
		reiser4_backup_close(fs->backup);
	}
#endif
	
	/* Freeing memory occupied by fs instance */
	aal_free(fs);
}

#ifndef ENABLE_MINIMAL
static errno_t cb_check_block(blk_t start, count_t width, void *data) {
	blk_t blk = *(blk_t *)data;
	return (blk >= start && blk < start + width);
}

/* Returns passed @blk owner */
reiser4_owner_t reiser4_fs_belongs(reiser4_fs_t *fs, blk_t blk) {
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
errno_t reiser4_fs_layout(reiser4_fs_t *fs, region_func_t func, void *data) {
	errno_t res;

	if ((res = reiser4_master_layout(fs->master, func, data)))
		return res;

	if ((res = reiser4_oid_layout(fs->oid, func, data)))
		return res;
	
	if ((res = reiser4_format_layout(fs->format, func, data)))
		return res;

	if (fs->journal) {
		if ((res = reiser4_journal_layout(fs->journal,
						  func, data)))
		{
			return res;
		}
	}
	
	if ((res = reiser4_status_layout(fs->status, func, data)))
		return res;

	if ((res = reiser4_alloc_layout(fs->alloc, func, data)))
		return res;

	return reiser4_backup_layout(fs, func, data);
}

static errno_t cb_mark_block(blk_t start, count_t width, void *data) {
	return reiser4_alloc_occupy((reiser4_alloc_t *)data,
				    start, width);
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
		
	if (!(fs->master = reiser4_master_create(device, hint)))
		goto error_free_fs;
	
	if (reiser4_format_check_len(device, hint->blksize, hint->blocks))
		goto error_free_master;

	/* Setting up master super block. */
	reiser4_master_set_format(fs->master, format->id.id);

	if (!(fs->status = reiser4_status_create(device, hint->blksize)))
		goto error_free_master;

	/* Getting tail policy from default params. */
	policy = reiser4_profile_plug(PROF_POLICY);
	
	/* Taking care about key flags in format super block */
	key = reiser4_profile_plug(PROF_KEY);
	
	/* Creates disk format. */
	if (!(fs->format = reiser4_format_create(fs, format, policy->id.id, 
						 key->id.id, hint->blocks)))
	{
		goto error_free_status;
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
	reiser4_tree_close(fs->tree);
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
errno_t reiser4_fs_backup(reiser4_fs_t *fs, backup_hint_t *hint) {
	errno_t res;

	aal_assert("vpf-1392", fs != NULL);
	aal_assert("vpf-1392", hint != NULL);

	/* Set the backup version. */
	((char *)hint->block.data)[0] = 0;

	/* Master backup starts on 1st byte. Note: Every backuper must set 
	   hint->off[next index] correctly. */
	hint->off[BK_MASTER] = 1;
	
	/* Backup the master. */
	if ((res = reiser4_master_backup(fs->master, hint)))
		return res;

	/* Backup the format. */
	return reiser4_format_backup(fs->format, hint);
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
errno_t reiser4_fs_sync(reiser4_fs_t *fs) {
	errno_t res;
	
	aal_assert("umka-231", fs != NULL);
   
	/* Synchronizing the tree */
	if ((res = reiser4_tree_sync(fs->tree)))
		return res;
    
	if (fs->journal && (res = reiser4_journal_sync(fs->journal)))
		return res;
	
	/* Synchronizing block allocator */
	if ((res = reiser4_alloc_sync(fs->alloc)))
		return res;
    
	/* Synchronizing the object allocator */
	if ((res = reiser4_oid_sync(fs->oid)))
		return res;
  
	if (fs->backup && (res = reiser4_backup_sync(fs->backup)))
		return res;
  
	if ((res = reiser4_format_sync(fs->format)))
		return res;

	if ((res = reiser4_status_sync(fs->status)))
		return res;

	return reiser4_master_sync(fs->master);
}

#endif

