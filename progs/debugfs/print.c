/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- filesystem print related code. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "debugfs.h"

/* Prints passed @buff into stdout. The special print function is needed because
   we can't just put 4k buffer into stdout. */
errno_t debugfs_print_buff(void *buff, uint32_t size) {
	int len = size;
	void *ptr = buff;

	while (len > 0) {
		int written;

		if ((written = write(1, ptr, len)) <= 0) {
			
			if (errno == EINTR)
				continue;
			
			return -EIO;
		}
		
		ptr += written;
		len -= written;
	}

	return 0;
}

errno_t debugfs_print_stream(aal_stream_t *stream) {
	char buff[256];

	aal_stream_reset(stream);
	
	while (stream->offset < stream->size) {
		uint32_t size;

		size = (stream->size - stream->offset);
		
		if (size > sizeof(buff))
			size = sizeof(buff) - size;

		if ((size = aal_stream_read(stream, buff, size)) <= 0)
			return size;
		
		if (debugfs_print_buff(buff, size))
			return -EIO;
	}

	return 0;
}

/* Prints passed @node. */
static errno_t tprint_process_node(reiser4_tree_t *tree,
				   node_t *node, void *data)
{
	errno_t res;
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);

	if ((res = repair_node_print(node, &stream)))
		goto error_free_stream;

	aal_stream_fini(&stream);
	
	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

errno_t debugfs_print_node(node_t *node) {
	return tprint_process_node(NULL, node, NULL);
}

/* Prints block denoted as blk */
errno_t debugfs_print_block(
	reiser4_fs_t *fs,           /* filesystem for work with */
	blk_t blk)                  /* block number to be printed */
{
	errno_t res;
	node_t *node;
	count_t blocks;

	if (blk >= (blocks = reiser4_format_get_len(fs->format))) {
		aal_error("Block %llu is out of filesystem "
			  "size %llu-%llu.", blk, (uint64_t)0,
			  (uint64_t)blocks);
		return -EINVAL;
	}
	
	/* Check if @blk is a filesystem block at all */
	if (!reiser4_alloc_occupied(fs->alloc, blk, 1)) {
		fprintf(stdout, "Block %llu is marked as not "
			"used.\n", blk);
	}

	/* Determining what is the object block belong to */
	switch (reiser4_fs_belongs(fs, blk)) {
	case O_MASTER:
		fprintf(stdout, "Block %llu is filesystem "
			"master super block.\n", blk);
		return 0;
	case O_STATUS:
		fprintf(stdout, "Block %llu is filesystem "
			"status block.\n", blk);
		return 0;
	case O_FORMAT:
		fprintf(stdout, "Block %llu belongs to format "
			"metadata.\n", blk);
		return 0;
	case O_JOURNAL:
		fprintf(stdout, "Block %llu belongs to journal "
			"metadata.\n", blk);
		return 0;
	case O_ALLOC:
		fprintf(stdout, "Block %llu belongs to block "
			"allocator metadata.\n", blk);
		return 0;
	case O_BACKUP:
		fprintf(stdout, "Block %llu belongs to the fs "
			"metadata backup.\n", blk);
		return 0;

	default:
		break;
	}
	
	if (!(node = reiser4_node_open(fs->tree, blk))) {
		fprintf(stdout, "Block %llu is used, but it is not "
			"a formatted one.\n", blk);
		return 0;
	}
	
	if ((res = debugfs_print_node(node)))
		return res;
	
	reiser4_node_close(node);
	return 0;
}

/* Makes traverse though the whole tree and prints all nodes */
errno_t debugfs_print_tree(reiser4_fs_t *fs) {
	aal_assert("umka-2486", fs != NULL);
	
	return reiser4_tree_trav(fs->tree, NULL,
				 tprint_process_node,
				 NULL, NULL, NULL);
}

/* Prints master super block */
errno_t debugfs_print_master(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;
	
	aal_assert("umka-1299", fs != NULL);

	aal_stream_init(&stream, stdout, &file_stream);
		
	if ((res = repair_master_print(fs->master, &stream, 
				       misc_uuid_unparse)))
	{
		return res;
	}

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
	
	return 0;
}

/* Prints fs status block. */
errno_t debugfs_print_status(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;
	
	aal_assert("umka-2495", fs != NULL);

	aal_stream_init(&stream, stdout, &file_stream);
		
	if ((res = repair_status_print(fs->status, &stream)))
		return res;

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
	
	return 0;
}

/* Prints format-specific super block */
errno_t debugfs_print_format(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->format->entity->plug->o.format_ops->print) {
		aal_info("Format print method is not "
			 "implemented.");
		return 0;
	}
    
	aal_stream_init(&stream, stdout, &file_stream);
	repair_format_print(fs->format, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
	
    	return 0;
}

/* Prints oid allocator */
errno_t debugfs_print_oid(reiser4_fs_t *fs) {
	aal_stream_t stream;
    
	if (!fs->oid->entity->plug->o.oid_ops->print) {
		aal_info("Oid allocator print method is "
			 "not implemented.");
		return 0;
	}

	aal_stream_init(&stream, stdout, &file_stream);
	repair_oid_print(fs->oid, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
	
    	return 0;
}

/* Prints block allocator */
errno_t debugfs_print_alloc(reiser4_fs_t *fs) {
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	repair_alloc_print(fs->alloc, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
	
    	return 0;
}

/* Prints journal */
errno_t debugfs_print_journal(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->journal)
		return -EINVAL;

	aal_stream_init(&stream, stdout, &file_stream);
	repair_journal_print(fs->journal, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
	
    	return 0;
}

struct fprint_hint {
	blk_t old;
	void *data;
	uint32_t flags;
};

typedef struct fprint_hint fprint_hint_t;

/* Prints item at passed @place */
static errno_t fprint_process_place(
	place_t *place,            /* next file block */
	void *data)                /* user-specified data */
{
	fprint_hint_t *hint = (fprint_hint_t *)data;

	if (node_blocknr(place->node) == hint->old)
		return 0;

	hint->old = node_blocknr(place->node);
	return debugfs_print_node(place->node);
}

/* Prints all items belong to the specified file */
errno_t debugfs_print_file(
	reiser4_fs_t *fs,          /* fs to be used */
	char *filename,            /* file name to be used */
	uint32_t flags)            /* some flags */
{
	errno_t res = 0;
	fprint_hint_t hint;
	reiser4_object_t *object;
	
	if (!(object = reiser4_object_open(fs->tree, filename, FALSE)))
		return -EINVAL;

	/* If --print-file option is specified, we show only items belong to the
	   file. If no, that we show all items whihc lie in the same block as
	   the item belong to the file denoted by @filename. */
	if (PF_ITEMS & flags) {
		aal_stream_t stream;

		aal_stream_init(&stream, stdout, &file_stream);
		repair_object_print(object, &stream);
		aal_stream_fini(&stream);
	} else {
		place_func_t place_func;
		
		hint.old = 0;
		hint.data = fs;
		hint.flags = flags;

		place_func = fprint_process_place;
		
		if ((res = reiser4_object_metadata(object, place_func, &hint))) {
			aal_error("Can't print object %s metadata.", object->name);
		}
	}

	reiser4_object_close(object);
	return res;
}

