/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   meta.c -- reiser4 metadata fetch/load stuff. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include "debugfs.h"

#define TREE_SIGN "TREE"

static errno_t debugfs_pack_tree(reiser4_fs_t *fs,
				 aal_stream_t *stream)
{
	blk_t blk;
	count_t len;
	errno_t res;

	/* Packing tree. */
	aal_stream_write(stream, TREE_SIGN, 4);
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

errno_t debugfs_pack_metadata(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;

	/* Stream init. */
	aal_stream_init(&stream);

	/* Packing master. */
	if ((res = reiser4_master_pack(fs->master, &stream)))
		return res;
	
	/* Packing format. */
	if ((res = reiser4_format_pack(fs->format, &stream)))
		return res;
	
	/* Packing block allocator. */
	if ((res = reiser4_alloc_pack(fs->alloc, &stream)))
		return res;
	
	/* Packing status block. */
	if ((res = reiser4_status_pack(fs->status, &stream)))
		return res;
	
	/* Packing tree. */
	if ((res = debugfs_pack_tree(fs, &stream)))
		return res;
	
	/* Print @stream to stdout. */
	if ((res = debugfs_print_stream(&stream)))
		return res;

	aal_stream_fini(&stream);
	return res;
}

errno_t debugfs_unpack_metadata(reiser4_fs_t *fs) {
	return 0;
}
