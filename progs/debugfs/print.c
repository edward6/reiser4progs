/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   print.c -- filesystem print related code. */

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "debugfs.h"

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
		
		printf(buff);
	}

	return 0;
}

/* Prints passed @node. */
static errno_t tprint_process_node(reiser4_tree_t *tree,
				   reiser4_node_t *node, void *data)
{
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);

	repair_node_print(node, &stream);

	aal_stream_fini(&stream);
	
	return 0;
}

void debugfs_print_node(reiser4_node_t *node) {
	tprint_process_node(NULL, node, NULL);
}

/* Prints block denoted as blk */
errno_t debugfs_print_block(
	reiser4_fs_t *fs,           /* filesystem for work with */
	blk_t blk)                  /* block number to be printed */
{
	count_t blocks;
	reiser4_node_t *node;

	if (blk >= (blocks = reiser4_format_get_len(fs->format))) {
		aal_error("Block %llu is out of filesystem "
			  "size %llu-%llu.", blk, (uint64_t)0,
			  (uint64_t)blocks);
		return -EINVAL;
	}
	
	/* Check if @blk is a filesystem block at all */
	fprintf(stdout, "Block %llu is marked as %sused.\n", blk,
		reiser4_alloc_occupied(fs->alloc, blk, 1) ? "" : "not ");

	/* Determining what is the object block belong to */
	switch (reiser4_fs_belongs(fs, blk)) {
	case O_MASTER:
		fprintf(stdout, "It belongs to the fs master super block.\n");
		return 0;
	case O_STATUS:
		fprintf(stdout, "It belongs to the fs status data.\n");
		return 0;
	case O_FORMAT:
		fprintf(stdout, "It belongs to the fs disk format data.\n");
		return 0;
	case O_JOURNAL:
		fprintf(stdout, "It belongs to the fs journal data.\n");
		return 0;
	case O_ALLOC:
		fprintf(stdout, "It belongs to the fs block allocator data.\n");
		return 0;
	case O_BACKUP:
		fprintf(stdout, "It belongs to the fs backup data.\n");
		return 0;

	default:
		break;
	}
	
	if (!(node = reiser4_node_open(fs->tree, blk))) {
		fprintf(stdout, "It does not look like a formatted one.\n");
		return 0;
	}
	
	debugfs_print_node(node);
	
	reiser4_node_close(node);
	
	return 0;
}

/* Makes traverse though the whole tree and prints all nodes */
void debugfs_print_tree(reiser4_fs_t *fs) {
	aal_assert("umka-2486", fs != NULL);
	
	reiser4_tree_trav(fs->tree, NULL, 
			  tprint_process_node,
			  NULL, NULL, NULL);
}

/* Prints master super block */
void debugfs_print_master(reiser4_fs_t *fs) {
	aal_stream_t stream;
	
	aal_assert("umka-1299", fs != NULL);

	aal_stream_init(&stream, stdout, &file_stream);
		
	repair_master_print(fs->master, &stream, misc_uuid_unparse);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/* Prints fs status block. */
void debugfs_print_status(reiser4_fs_t *fs) {
	aal_stream_t stream;
	
	aal_assert("umka-2495", fs != NULL);

	aal_stream_init(&stream, stdout, &file_stream);
		
	repair_status_print(fs->status, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/* Prints format-specific super block */
void debugfs_print_format(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->format->ent->plug->o.format_ops->print) {
		aal_info("Format print method is not "
			 "implemented.");
		return;
	}
    
	aal_stream_init(&stream, stdout, &file_stream);
	repair_format_print(fs->format, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/* Prints oid allocator */
void debugfs_print_oid(reiser4_fs_t *fs) {
	aal_stream_t stream;
    
	if (!fs->oid->ent->plug->o.oid_ops->print) {
		aal_info("Oid allocator print method is "
			 "not implemented.");
		return;
	}

	aal_stream_init(&stream, stdout, &file_stream);
	repair_oid_print(fs->oid, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/* Prints block allocator */
void debugfs_print_alloc(reiser4_fs_t *fs) {
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	repair_alloc_print(fs->alloc, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

/* Prints journal */
void debugfs_print_journal(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->journal)
		return;

	aal_stream_init(&stream, stdout, &file_stream);
	repair_journal_print(fs->journal, &stream);

	aal_stream_format(&stream, "\n");
	aal_stream_fini(&stream);
}

struct fprint_hint {
	blk_t old;
	void *data;
	uint32_t flags;
};

typedef struct fprint_hint fprint_hint_t;

/* Prints item at passed @place */
static errno_t fprint_process_place(
	reiser4_place_t *place,    /* next file block */
	void *data)                /* user-specified data */
{
	fprint_hint_t *hint = (fprint_hint_t *)data;

	if (place_blknr(place) == hint->old)
		return 0;

	hint->old = place_blknr(place);
	debugfs_print_node(place->node);

	return 0;
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
	
	if (!(object = reiser4_semantic_open(fs->tree, filename, NULL, 0)))
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

