/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   filesystem.c -- common reiser4 filesystem code. */

#include <reiser4/reiser4.h>

/* Opens filesystem on specified device */
reiser4_fs_t *reiser4_fs_open(aal_device_t *device,
			      bool_t check)
{
#ifndef ENABLE_STAND_ALONE
	uint64_t flags;
	count_t blocks;
	uint32_t blksize;
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
	flags = plug_call(fs->format->entity->plug->o.format_ops,
			  get_flags, fs->format->entity);
	
	if ((1 << REISER4_LARGE_KEYS) & flags) {
		reiser4_param_override("key", "key_large");
	} else {
		reiser4_param_override("key", "key_short");
	}
	
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
			aal_exception_warn("Block allocator data "
					   "seems corrupted.");
		}
	}
	
	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_alloc;
  
	if (check) {
		if (reiser4_oid_valid(fs->oid))
			goto error_free_oid;
	}
#endif
	
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_oid;
	
	return fs;

 error_free_oid:
#ifndef ENABLE_STAND_ALONE
	reiser4_oid_close(fs->oid);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
	reiser4_format_close(fs->format);
#endif
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
void reiser4_fs_close(
	reiser4_fs_t *fs)		/* filesystem to be closed */
{
    
	aal_assert("umka-230", fs != NULL);

#ifndef ENABLE_STAND_ALONE
	if (!aal_device_readonly(fs->device))
		reiser4_fs_sync(fs);
#endif
	reiser4_tree_fini(fs->tree);
	
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
static errno_t callback_check_block(void *entity, blk_t start,
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

	/* Checks if passed @blk belongs to skipped area */
	if (reiser4_format_skipped(fs->format, callback_check_block, &blk))
		return O_SKIPPED;
	
	/* Checks if passed @blk is master super block */
	if (reiser4_master_layout(fs->master, callback_check_block, &blk))
		return O_MASTER;
	
	/* Checks if passed @blk belongs to format metadata */
	if (reiser4_format_layout(fs->format, callback_check_block, &blk))
		return O_FORMAT;

	/* Checks if passed @blk belongs to oid allocator metadata */
	if (reiser4_oid_layout(fs->oid, callback_check_block, &blk))
		return O_OID;

	/* Checks if passed @blk belongs to journal metadata if journal
	   opened. */
	if (fs->journal) {
		if (reiser4_journal_layout(fs->journal,
					   callback_check_block, &blk))
		{
			return O_JOURNAL;
		}
	}

	/* Check if @blk is filesystem status block. */
	if (reiser4_status_layout(fs->status, callback_check_block, &blk))
		return O_STATUS;
	
	/* Checks if passed @blk belongs to block allocator data */
	if (reiser4_alloc_layout(fs->alloc, callback_check_block, &blk))
		return O_ALLOC;

	return O_UNKNOWN;
}

/* Enumerates all filesystem areas (block alloc, journal, etc.) */
errno_t reiser4_fs_layout(reiser4_fs_t *fs,
			  region_func_t region_func, 
			  void *data)
{
	errno_t res;

	/* Enumerating skipped area */
	if ((res = reiser4_format_skipped(fs->format, region_func, data)))
		return res;
	
	/* Enumerating master area */
	if ((res = reiser4_master_layout(fs->master, region_func, data)))
		return res;

	/* Enumerating oid allocator area */
	if ((res = reiser4_oid_layout(fs->oid, region_func, data)))
		return res;
	
	/* Enumerating format area */
	if ((res = reiser4_format_layout(fs->format, region_func, data)))
		return res;

	/* Enumerating journal area */
	if (fs->journal) {
		if ((res = reiser4_journal_layout(fs->journal,
						  region_func, data)))
		{
			return res;
		}
	}
	
	if ((res = reiser4_status_layout(fs->status, region_func, data)))
		return res;

	/* Enumerating block allocator area */
	return reiser4_alloc_layout(fs->alloc, region_func, data);
}

static errno_t callback_mark_block(void *entity, blk_t start,
				   count_t width, void *data)
{
	return reiser4_alloc_occupy((reiser4_alloc_t *)data,
				    start, width);
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
    
	/* Checks whether filesystem size is enough big. */
	if (hint->blocks < REISER4_MIN_SIZE) {
		aal_exception_error("Requested filesytem size (%llu) "
				    "too small. Reiser4 required minimal "
				    "size %u blocks long.", hint->blocks,
				    REISER4_MIN_SIZE);
		return NULL;
	}
    
	/* Allocating memory and initializing fileds. */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;
	
	/* Create master super block. */
	format = reiser4_param_value("format");
		
	if (!(fs->master = reiser4_master_create(device, hint->blksize)))
		goto error_free_fs;

	/* Setting up master super block. */
	reiser4_master_set_format(fs->master, format);
	reiser4_master_set_uuid(fs->master, hint->uuid);
	reiser4_master_set_label(fs->master, hint->label);

	if (!(fs->status = reiser4_status_create(device, hint->blksize)))
		goto error_free_master;

	/* Getting tail policy from default params. */
	policy = reiser4_param_value("policy");
	
	/* Creates disk format. */
	if (!(fs->format = reiser4_format_create(fs, hint->blocks,
						 policy, format)))
	{
		goto error_free_status;
	}

	/* Taking care about key flags in format super block */
	if (reiser4_param_value("key") == KEY_LARGE_ID) {
		plug_call(fs->format->entity->plug->o.format_ops, set_flags,
			  fs->format->entity, (1 << REISER4_LARGE_KEYS));
	} else {
		plug_call(fs->format->entity->plug->o.format_ops,
			  set_flags, fs->format->entity, 0);
	}

	/* Creates block allocator */
	if (!(fs->alloc = reiser4_alloc_create(fs, hint->blocks)))
		goto error_free_format;

	/* Initializes oid allocator */
	if (!(fs->oid = reiser4_oid_create(fs)))
		goto error_free_alloc;
	
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_oid;
	
	if (reiser4_fs_layout(fs, callback_mark_block, fs->alloc)) {
		aal_exception_error("Can't mark filesystem blocks used.");
		goto error_free_tree;
	}

	free = reiser4_alloc_free(fs->alloc);
	reiser4_format_set_free(fs->format, free);

	return fs;

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
	
	return reiser4_tree_clone(src_fs->tree, dst_fs->tree);
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

errno_t reiser4_fs_pack(reiser4_fs_t *fs, aal_stream_t *stream) {
	blk_t blk;
	count_t len;
	errno_t res;
	
	aal_assert("umka-2630", fs != NULL);
	aal_assert("umka-2647", stream != NULL);

	aal_stream_write(stream, MASTER_PACK_SIGN, 4);

	if ((res = reiser4_master_pack(fs->master, stream)))
		return res;
	
	aal_stream_write(stream, FORMAT_PACK_SIGN, 4);
	
	if ((res = reiser4_format_pack(fs->format, stream)))
		return res;
	
	aal_stream_write(stream, ALLOC_PACK_SIGN, 4);
	
	if ((res = reiser4_alloc_pack(fs->alloc, stream)))
		return res;
	
	aal_stream_write(stream, STATUS_PACK_SIGN, 4);
	
	if ((res = reiser4_status_pack(fs->status, stream)))
		return res;
	
	len = reiser4_format_get_len(fs->format);

	/* Loop though the all data blocks, check if they belong to tree and if
	   so try to open a formated node on it. */
	for (blk = 0; blk < len; blk++) {
		reiser4_node_t *node;
		
		/* We're not interested in unused blocks yet. */
		if (!reiser4_alloc_occupied(fs->alloc, blk, 1))
			continue;

		/* We're not interested in other blocks, but tree nodes. */
		if (reiser4_fs_belongs(fs, blk) != O_UNKNOWN)
			continue;

		/* Try to open @blk block and find out is it formatted one or
		   not. */
		if (!(node = reiser4_node_open(fs->tree, blk)))
			continue;

		aal_stream_write(stream, NODE_PACK_SIGN, 4);
		
		/* Packing @node to @stream. */
		if ((res = reiser4_node_pack(node, stream)))
			return res;

		/* Close node. */
		reiser4_node_close(node);
	}

	return 0;
}

reiser4_fs_t *reiser4_fs_unpack(aal_device_t *device,
				aal_stream_t *stream)
{
	uint32_t bs;
	reiser4_fs_t *fs;
	char sign[5] = {0};

	aal_assert("umka-2633", device != NULL);
	aal_assert("umka-2648", stream != NULL);

	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;

	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_exception_error("Can't unpack master super "
				    "block. Stream is over?");
		goto error_free_fs;
	}

	if (aal_strncmp(sign, MASTER_PACK_SIGN, 4)) {
		aal_exception_error("Invalid master sign %s is "
				    "detected in stream.", sign);
		goto error_free_fs;
	}
	
	if (!(fs->master = reiser4_master_unpack(device, stream)))
		goto error_free_fs;

	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_exception_error("Can't unpack format super "
				    "block. Stream is over?");
		goto error_free_master;
	}

	if (aal_strncmp(sign, FORMAT_PACK_SIGN, 4)) {
		aal_exception_error("Invalid format sign %s is "
				    "detected in stream.", sign);
		goto error_free_master;
	}
	
	if (!(fs->format = reiser4_format_unpack(fs, stream)))
		goto error_free_master;

	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_format;
			
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_oid;
			
	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_exception_error("Can't unpack block "
				    "allocator. Stream is over?");
		goto error_free_tree;
	}

	if (aal_strncmp(sign, ALLOC_PACK_SIGN, 4)) {
		aal_exception_error("Invalid block alloc sign %s is "
				    "detected in stream.", sign);
		goto error_free_tree;
	}

	if (!(fs->alloc = reiser4_alloc_unpack(fs, stream)))
		goto error_free_tree;
			
	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_exception_error("Can't unpack status "
				    "block. Stream is over?");
		goto error_free_alloc;
	}

	if (aal_strncmp(sign, STATUS_PACK_SIGN, 4)) {
		aal_exception_error("Invalid status block sign %s is "
				    "detected in stream.", sign);
		goto error_free_alloc;
	}

	bs = reiser4_master_get_blksize(fs->master);
	
	if (!(fs->status = reiser4_status_unpack(device, bs,
						 stream)))
	{
		goto error_free_alloc;
	}

	while (1) {
		reiser4_node_t *node;
		
		if (aal_stream_read(stream, &sign, 4) != 4) {
			if (aal_stream_eof(stream)) {
				break;
			} else {
				goto error_free_status;
			}
		}

		if (!aal_strncmp(sign, NODE_PACK_SIGN, 4)) {
			if (!(node = reiser4_node_unpack(fs->tree, stream)))
				goto error_free_status;

			if (reiser4_node_sync(node)) {
				reiser4_node_close(node);
				goto error_free_status;
			}
		
			reiser4_node_close(node);
		} else {
			aal_exception_error("Invalid object %s is "
					    "detected in stream. "
					    "Node is expacted.",
					    sign);
			goto error_free_fs;
		}
	}

	return fs;

 error_free_status:
	reiser4_status_close(fs->status);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_tree:
	reiser4_tree_fini(fs->tree);
 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_format:
	reiser4_format_close(fs->format);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
	return NULL;
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
