/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   meta.c -- reiser4 metadata fetch/load stuff. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include "debugfs.h"

/* Writes tree nodes metadata to stdout. */
static errno_t debugfs_pack_tree(reiser4_fs_t *fs) {
	blk_t blk;
	count_t len;
	errno_t res;
	
	aal_stream_t stream;

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

		aal_stream_init(&stream);
		
		/* Packing @node to @stream. */
		if ((res = reiser4_node_pack(node, &stream))) {
			aal_stream_fini(&stream);
			return res;
		}

		if ((res = debugfs_print_stream(&stream))) {
			aal_stream_fini(&stream);
			return res;
		}

		aal_stream_fini(&stream);

		/* Close node. */
		reiser4_node_close(node);
	}

	return 0;
}

/* Write fs metadata except of tree to stdout. */
errno_t debugfs_pack_meta(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;

	aal_assert("umka-2630", fs != NULL);

	aal_stream_init(&stream);
	
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
	
	if ((res = debugfs_print_stream(&stream)))
		goto error_free_stream;
	
	aal_stream_fini(&stream);
	
	return debugfs_pack_tree(fs);

 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

reiser4_fs_t *debugfs_unpack_meta(aal_device_t *device) {
	reiser4_fs_t *fs;
	aal_stream_t stream;

	aal_assert("umka-2633", device != NULL);
	
	aal_stream_init(&stream);
	
	/* Allocating memory and initializing fileds */
	if (!(fs = aal_calloc(sizeof(*fs), 0)))
		goto error_free_stream;
	
	fs->device = device;

	if (!(fs->master = reiser4_master_unpack(device, &stream)))
		goto error_free_fs;

	if (!(fs->status = reiser4_status_unpack(device, &stream)))
		goto error_free_master;

	/* Creates disk format. */
	if (!(fs->format = reiser4_format_unpack(fs, &stream)))
		goto error_free_status;

	/* Creates block allocator. */
	if (!(fs->alloc = reiser4_alloc_unpack(fs, &stream)))
		goto error_free_format;

	/* FIXME-UMKA: Here shopuld also be nodes unpacking. */

	aal_stream_fini(&stream);
	return fs;

 error_free_format:
	reiser4_format_close(fs->format);
 error_free_status:
	reiser4_status_close(fs->status);
 error_free_master:
	reiser4_master_close(fs->master);
 error_free_fs:
	aal_free(fs);
 error_free_stream:
	aal_stream_fini(&stream);
	return NULL;
}
