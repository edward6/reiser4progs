/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   meta.c -- reiser4 metadata fetch/load stuff. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include "debugfs.h"

/* Writes tree nodes metadata to stdout. */
errno_t debugfs_pack_tree(reiser4_fs_t *fs) {
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
	
	return 0;

 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

errno_t debugfs_unpack_tree(reiser4_fs_t *fs) {
	return 0;
}

errno_t debugfs_unpack_meta(reiser4_fs_t *fs) {
	return 0;
}
