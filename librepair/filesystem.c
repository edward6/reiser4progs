/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/filesystem.c - methods are needed mostly by fsck for work 
   with broken filesystems. */

#include <repair/librepair.h>

/* Opens the filesystem - master, format, block and oid allocators - without 
   opening a journal. */
errno_t repair_fs_open(repair_data_t *repair, 
		       aal_device_t *host_device,
		       aal_device_t *journal_device)
{
	errno_t res = 0, error;
	uint64_t len;

	aal_assert("vpf-851", repair != NULL);
	aal_assert("vpf-159", host_device != NULL);
 
	/* Allocating memory and initializing fields */
	if (!(repair->fs = aal_calloc(sizeof(*repair->fs), 0)))
		return -ENOMEM;

	repair->fs->device = host_device;
	
	res |= repair_master_open(repair->fs, repair->mode);
	
	if (repair_error_fatal(res)) {
		aal_fatal("Failed to open the master super block.");
		goto error_fs_free;
	}
	
	res |= repair_format_open(repair->fs, repair->mode);
	
	if (repair_error_fatal(res)) {
		aal_fatal("Failed to open the format.");
		goto error_master_close;
	}

	/* FIXME-VITALY: if status has io flag set and there is no bad
	   block file given to fsck -- do not continue -- when bad block 
	   support will be written. */
	res |= repair_status_open(repair->fs, repair->mode);
	
	if (repair_error_fatal(res)) {
		aal_fatal("Failed to open the status block.");
		goto error_format_close;
	}
	
	res |= repair_journal_open(repair->fs, journal_device, repair->mode);
	
	if (repair_error_fatal(res)) {
		aal_fatal("Failed to open the journal.");
		goto error_status_close;
	}
	
	res |= repair_journal_replay(repair->fs->journal, repair->fs->device);
	
	if (repair_error_fatal(res)) {
		aal_fatal("Failed to replay the journal.");
		goto error_journal_close;
	}
	
	res |= repair_format_update(repair->fs->format);
	
	if (repair_error_fatal(res)) {
		aal_fatal("Failed to update the format after journal "
			  "replaying.");
		goto error_journal_close;
	}

	len = reiser4_format_get_len(repair->fs->format);

	/* Block and oid allocator plugins are specified by format plugin 
	 * unambiguously, so there is nothing to be checked here anymore. */
	if (!(repair->fs->alloc = reiser4_alloc_open(repair->fs, len))) {
		aal_fatal("Failed to open a block allocator.");
		res = -EINVAL;
		goto error_journal_close;
	}
	
	if ((res |= error = reiser4_alloc_valid(repair->fs->alloc)) < 0)
		goto error_alloc_close;
	
	if (error && repair->mode != RM_CHECK)
		aal_mess("Checksums will be fixed later.\n");
		
	if (!(repair->fs->oid = reiser4_oid_open(repair->fs))) {	
		aal_fatal("Failed to open an object id allocator.");
		res = -EINVAL;
		goto error_alloc_close;
	}

	if (!(repair->fs->tree = reiser4_tree_init(repair->fs))) {
		res = -ENOMEM;
		goto error_oid_close;
	}
		
	
	repair_error_count(repair, res);
	return 0;

 error_oid_close:
	reiser4_tree_fini(repair->fs->tree);
	
 error_alloc_close:
	reiser4_alloc_close(repair->fs->alloc);
	repair->fs->alloc = NULL;
    
 error_journal_close:
	reiser4_journal_close(repair->fs->journal);
	repair->fs->journal = NULL;

 error_status_close:
	reiser4_status_close(repair->fs->status);
	repair->fs->status = NULL;

 error_format_close:
	reiser4_format_close(repair->fs->format);
	repair->fs->format = NULL;

 error_master_close:
	reiser4_master_close(repair->fs->master);
	repair->fs->master = NULL;

 error_fs_free:
	aal_free(repair->fs);
	repair->fs = NULL;

	repair_error_count(repair, res);
	return res < 0 ? res : 0;
}

/* Close the journal and the filesystem. */
void repair_fs_close(reiser4_fs_t *fs) {
	aal_assert("vpf-909", fs != NULL);
	aal_assert("vpf-910", fs->journal != NULL);
	
	reiser4_journal_close(fs->journal);
	reiser4_fs_close(fs);    
}

/* Pack passed @fs to @stream. */
errno_t repair_fs_pack(reiser4_fs_t *fs, aal_stream_t *stream) {
	count_t len;
	errno_t res;
	blk_t blk;
	
	aal_assert("umka-2630", fs != NULL);
	aal_assert("umka-2647", stream != NULL);

	aal_stream_write(stream, MASTER_PACK_SIGN, 4);

	if ((res = repair_master_pack(fs->master, stream)))
		return res;
	
	aal_stream_write(stream, FORMAT_PACK_SIGN, 4);
	
	if ((res = repair_format_pack(fs->format, stream)))
		return res;
	
	aal_stream_write(stream, ALLOC_PACK_SIGN, 4);
	
	if ((res = repair_alloc_pack(fs->alloc, stream)))
		return res;
	
	aal_stream_write(stream, STATUS_PACK_SIGN, 4);
	
	if ((res = repair_status_pack(fs->status, stream)))
		return res;
	
	aal_stream_write(stream, BACKUP_PACK_SIGN, 4);
	
	if ((res = repair_backup_pack(fs, stream)))
		return res;
	
	len = reiser4_format_get_len(fs->format);

	/* Loop though the all data blocks, check if they belong to tree and if
	   so try to open a formated node on it. */
	for (blk = 0; blk < len; blk++) {
		node_t *node;
		errno_t res;
		
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

		res = plug_call(node->entity->plug->o.node_ops, check_struct,
				node->entity, RM_CHECK);
		
		if (res < 0) return res;

		if (res) {
			aal_stream_write(stream, BLOCK_PACK_SIGN, 4);
		
			/* Packing @node to @stream. */
			if ((res = repair_node_pack(node, stream, PACK_OFF)))
				return res;
		} else {
			aal_stream_write(stream, NODE_PACK_SIGN, 4);
		
			/* Packing @node to @stream. */
			if ((res = repair_node_pack(node, stream, PACK_FULL)))
				return res;
		}

		/* Close node. */
		reiser4_node_close(node);
	}

	return 0;
}

/* Unpack filesystem from @stream to @device. */
reiser4_fs_t *repair_fs_unpack(aal_device_t *device,
			       aal_stream_t *stream)
{
	uint64_t bn;
	uint32_t bs;
	reiser4_fs_t *fs;
	char sign[5] = {0};
	
	aal_block_t *block;
	
	aal_assert("umka-2633", device != NULL);
	aal_assert("umka-2648", stream != NULL);

	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		return NULL;
	
	fs->device = device;

	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_error("Can't unpack master super block. Stream is over?");
		goto error_free_fs;
	}

	if (aal_strncmp(sign, MASTER_PACK_SIGN, 4)) {
		aal_error("Invalid master sign %s is "
			  "detected in stream.", sign);
		goto error_free_fs;
	}
	
	if (!(fs->master = repair_master_unpack(device, stream)))
		goto error_free_fs;

	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_error("Can't unpack format super block. Stream is over?");
		goto error_free_master;
	}

	if (aal_strncmp(sign, FORMAT_PACK_SIGN, 4)) {
		aal_error("Invalid format sign %s is "
			  "detected in stream.", sign);
		goto error_free_master;
	}
	
	if (!(fs->format = repair_format_unpack(fs, stream)))
		goto error_free_master;

	/* Write into the very last block on the fs to make the output 
	   of the proper size. */
	bn = reiser4_format_get_len(fs->format) - 1;
	bs = reiser4_master_get_blksize(fs->master);

	if (!(block = aal_block_alloc(device, bs, bn))) {
		aal_error("Can't allocate the very last block (%llu) "
			  "on the fs: %s", bn, device->error);
		goto error_free_format;
	}

	if (aal_block_write(block)) {
		aal_error("Can't write the very last block (%llu) "
			  "on the fs: %s", bn, device->error);
		aal_free(block);
		goto error_free_format;
	}

	aal_free(block);
	
	if (!(fs->oid = reiser4_oid_open(fs)))
		goto error_free_format;
			
	if (!(fs->tree = reiser4_tree_init(fs)))
		goto error_free_oid;
			
	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_error("Can't unpack block allocator. Stream is over?");
		goto error_free_tree;
	}

	if (aal_strncmp(sign, ALLOC_PACK_SIGN, 4)) {
		aal_error("Invalid block alloc sign %s is "
			  "detected in stream.", sign);
		goto error_free_tree;
	}

	if (!(fs->alloc = repair_alloc_unpack(fs, stream)))
		goto error_free_tree;
			
	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_error("Can't unpack status block. Stream is over?");
		goto error_free_alloc;
	}

	if (aal_strncmp(sign, STATUS_PACK_SIGN, 4)) {
		aal_error("Invalid status block sign %s is "
			  "detected in stream.", sign);
		goto error_free_alloc;
	}

	if (!(fs->status = repair_status_unpack(device, bs, stream)))
		goto error_free_alloc;

	if (aal_stream_read(stream, &sign, 4) != 4) {
		aal_error("Can't unpack backup blocks. Stream is over?");
		goto error_free_status;
	}

	if (aal_strncmp(sign, BACKUP_PACK_SIGN, 4)) {
		aal_error("Invalid backup blocks sign %s is "
			  "detected in stream.", sign);
		goto error_free_status;
	}

	if (repair_backup_unpack(fs, stream))
		goto error_free_status;

	while (1) {
		node_t *node;
		
		if (aal_stream_read(stream, &sign, 4) != 4) {
			if (aal_stream_eof(stream))
				break;
			else
				goto error_free_status;
		}

		if (!aal_strncmp(sign, NODE_PACK_SIGN, 4)) {
			node = repair_node_unpack(fs->tree, stream, PACK_FULL);
		} else if (!aal_strncmp(sign, BLOCK_PACK_SIGN, 4)) {
			node = repair_node_unpack(fs->tree, stream, PACK_OFF);
		} else {
			aal_error("Invalid object %s is detected in stream. "
				  "Node is expected.", sign);
			goto error_free_status;
		}

		if (!node) goto error_free_status;

		if (reiser4_node_sync(node)) {
			reiser4_node_close(node);
			goto error_free_status;
		}

		reiser4_node_close(node);
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

