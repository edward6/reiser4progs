/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   meta.c -- reiser4 metadata pack/unpack stuff. */

#include <stdio.h>
#include "debugfs.h"

/* Writes tree nodes metadata to stdout. */
static errno_t debugfs_pack_tree(reiser4_fs_t *fs, aal_stream_t *stream) {
	blk_t blk;
	count_t len;
	errno_t res;
	
	/* Packing tree. */
	len = reiser4_format_get_len(fs->format);
	
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

		/* Packing @node to @stream. */
		if ((res = reiser4_node_pack(node, stream)))
			return res;

		/* Close node. */
		reiser4_node_close(node);
	}

	return 0;
}

/* Write fs metadata except of tree to stdout. */
errno_t debugfs_pack_meta(reiser4_fs_t *fs, char *filename) {
	FILE *file;
	errno_t res;
	aal_stream_t stream;

	aal_assert("umka-2630", fs != NULL);

	if (filename != NULL) {
		if (!(file = fopen(filename, "w+"))) {
			aal_exception_error("Can't create file "
					    "%s.", filename);
			return -EIO;
		}
	} else {
		file = stdout;
	}
	
	aal_stream_init(&stream, file, &file_stream);
	
	/* Packing master. */
	if ((res = reiser4_master_pack(fs->master, &stream)))
		goto error_free_stream;
	
	/* Packing format. */
	if ((res = reiser4_format_pack(fs->format, &stream)))
		goto error_free_stream;
	
	/* Packing block allocator. */
	if ((res = reiser4_alloc_pack(fs->alloc, &stream)))
		goto error_free_stream;
	
	/* Packing status block. */
	if ((res = reiser4_status_pack(fs->status, &stream)))
		goto error_free_stream;
	
	if ((res = debugfs_pack_tree(fs, &stream)))
		goto error_free_stream;

	aal_stream_fini(&stream);

	if (filename != NULL)
		fclose(file);
	
	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

/* Reads tree nodes metadata from stdin. */
static errno_t debugfs_unpack_tree(reiser4_fs_t *fs,
				   aal_stream_t *stream)
{
	errno_t res = 0;
	reiser4_node_t *node;

	/* Initializing tree. */
	if (!(fs->oid = reiser4_oid_open(fs)))
		return -EINVAL;
	
	if (!(fs->tree = reiser4_tree_init(fs, NULL))) {
		res = -EINVAL;
		goto error_free_oid;
	}
	
	while (!aal_stream_eof(stream)) {
		if (!(node = reiser4_node_unpack(fs->tree, stream))) {
			res = -EIO;
			goto error_free_tree;
		}

		if ((res = reiser4_node_sync(node))) {
			reiser4_node_close(node);
			goto error_free_tree;
		}
		
		reiser4_node_close(node);
	}
	
	return 0;

 error_free_oid:
	reiser4_oid_close(fs->oid);
 error_free_tree:
	reiser4_tree_fini(fs->tree);
	return res;
}

reiser4_fs_t *debugfs_unpack_meta(aal_device_t *device,
				  char *filename)
{
	FILE *file;
	reiser4_fs_t *fs;
	aal_stream_t stream;

	aal_assert("umka-2633", device != NULL);

	if (filename != NULL) {
		if (!(file = fopen(filename, "r"))) {
			aal_exception_error("Can't open file "
					    "%s.", filename);
			return NULL;
		}
	} else {
		file = stdin;
	}
	
	/* Initializing @stream. */
	aal_stream_init(&stream, file, &file_stream);
	
	/* Allocating memory and initializing fileds */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		goto error_free_stream;
	
	fs->device = device;

	if (!(fs->master = reiser4_master_unpack(device, &stream)))
		goto error_free_fs;

	/* Creates disk format. */
	if (!(fs->format = reiser4_format_unpack(fs, &stream)))
		goto error_free_master;

	/* Creates block allocator. */
	if (!(fs->alloc = reiser4_alloc_unpack(fs, &stream)))
		goto error_free_format;

	if (!(fs->status = reiser4_status_unpack(device, &stream)))
		goto error_free_alloc;

	if (debugfs_unpack_tree(fs, &stream))
		goto error_free_status;

	aal_stream_fini(&stream);

	if (filename != NULL)
		fclose(file);
		
	return fs;

 error_free_status:
	reiser4_status_close(fs->status);
 error_free_alloc:
	reiser4_alloc_close(fs->alloc);
 error_free_format:
	reiser4_format_close(fs->format);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
 error_free_stream:
	aal_stream_fini(&stream);
	return NULL;
}
