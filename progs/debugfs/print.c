/*
  print.c -- filesystem print related code.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#include "debugfs.h"

/* Callback function used in traverse for opening the node */
static errno_t tprint_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* block node lies in */
	void *data)		    /* traverse data */
{
	reiser4_tree_t *tree = (reiser4_tree_t *)data;
	aal_device_t *device = tree->fs->device;

	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

/* Prints passed @node */
static errno_t tprint_process_node(
	reiser4_node_t *node,	    /* node to be printed */
	void *data)		    /* traverse data */
{
	errno_t res;
	aal_stream_t stream;
	aal_stream_init(&stream);

	if ((res = reiser4_node_print(node, &stream)))
		goto error_free_stream;

	debugfs_print_stream(&stream);
	aal_stream_fini(&stream);
	
	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

errno_t debugfs_print_node(reiser4_node_t *node) {
	return tprint_process_node(node, NULL);
}

/* Prints block denoted as blk */
errno_t debugfs_print_block(
	reiser4_fs_t *fs,           /* filesystem for work with */
	blk_t blk)                  /* block number to be printed */
{
	count_t blocks;
	errno_t res = 0;
	aal_device_t *device;
	reiser4_node_t *node;

	if (blk >= (blocks = reiser4_format_get_len(fs->format))) {
		aal_exception_error("Block %llu is out of filesystem "
				    "size %llu-%llu.", blk, (uint64_t)0,
				    (uint64_t)blocks);
		return -EINVAL;
	}
	
	/* Check if @blk is a filesystem block at all */
	if (!reiser4_alloc_occupied(fs->alloc, blk, 1)) {
		aal_exception_info("Block %llu is not used.", blk);
		return 0;
	}

	/* Determining what is the object block belong to */
	switch (reiser4_fs_belongs(fs, blk)) {
	case O_SKIPPED:
		aal_exception_info("Block %llu belongs to skipped area "
				   "in the begin of partition.", blk);
		return 0;
	case O_FORMAT:
		aal_exception_info("Block %llu belongs to format "
				   "metadata.", blk);
		return 0;
	case O_JOURNAL:
		aal_exception_info("Block %llu belongs to journal "
				   "metadata.", blk);
		return 0;
	case O_ALLOC:
		aal_exception_info("Block %llu belongs to block "
				   "allocator metadata.", blk);
		return 0;
	default:
		break;
	}
	
	device = fs->device;

	/*
	  If passed @blk points to a formatted node then open it and print
	  using print_process_node listed abowe.
	*/
	if (!(node = reiser4_node_open(device, blk))) {
		aal_exception_info("Block %llu is not a formatted "
				   "one.", blk);
		return 0;
	}
	
	if ((res = debugfs_print_node(node)))
		return res;
	
	reiser4_node_close(node);
	
	return 0;
}

/* Makes traverse though the whole tree and prints all nodes */
errno_t debugfs_print_tree(reiser4_fs_t *fs) {
	traverse_hint_t hint;
	
	hint.cleanup = 1;
	hint.data = fs->tree;
	
	return reiser4_tree_traverse(fs->tree, &hint, tprint_open_node, 
				     tprint_process_node, NULL, NULL, NULL);
}

/* Prints master super block */
errno_t debugfs_print_master(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;
	
	aal_assert("umka-1299", fs != NULL);

	aal_stream_init(&stream);
		
	if ((res = reiser4_master_print(fs->master, &stream)))
		return res;

	/*
	  If reiser4progs supports uuid (if it was found durring building), then
	  it will also print uuid stored in master super block.
	*/
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	{
		char uuid[37];

		if (aal_strlen(reiser4_master_uuid(fs->master)))
			uuid_unparse(reiser4_master_uuid(fs->master), uuid);
		else
			aal_strncpy(uuid, "<none>", sizeof(uuid));
		
		aal_stream_format(&stream, "uuid:\t\t%s\n", uuid);
	}
#endif

	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);
	
	aal_stream_fini(&stream);
	return 0;
}

/* Prints format-specific super block */
errno_t debugfs_print_format(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;

	if (!fs->format->entity->plugin->format_ops.print) {
		aal_exception_info("Format print method is not "
				   "implemented.");
		return 0;
	}
    
	aal_stream_init(&stream);
	
	if ((res = reiser4_format_print(fs->format, &stream))) {
		aal_exception_error("Can't print format specific "
				    "super block.");
		goto error_free_stream;
	}
    
	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

/* Prints oid allocator */
errno_t debugfs_print_oid(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;
    
	if (!fs->oid->entity->plugin->oid_ops.print) {
		aal_exception_info("Oid allocator print method is "
				   "not implemented.");
		return 0;
	}

	aal_stream_init(&stream);

	if ((res = reiser4_oid_print(fs->oid, &stream))) {
		aal_exception_error("Can't print oid allocator.");
		goto error_free_stream;;
	}

	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

/* Prints block allocator */
errno_t debugfs_print_alloc(reiser4_fs_t *fs) {
	errno_t res;
	aal_stream_t stream;

	aal_stream_init(&stream);
    
	if ((res = reiser4_alloc_print(fs->alloc, &stream))) {
		aal_exception_error("Can't print block allocator.");
		goto error_free_stream;;
	}

	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return res;
}

/* Prints journal */
errno_t debugfs_print_journal(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, journal print is not "
			    "implemented yet!");
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
	object_entity_t *entity,   /* file to be inspected */
	place_t *place,            /* next file block */
	void *data)                /* user-specified data */
{
	errno_t res;
	fprint_hint_t *hint = (fprint_hint_t *)data;
	reiser4_place_t *p = (reiser4_place_t *)place;

	if (p->node->blk == hint->old)
		return 0;

	hint->old = p->node->blk;
	
	if ((res = debugfs_print_node(p->node))) {
		aal_exception_error("Can't print node %llu.",
				    hint->old);
		return res;
	}

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
	
	if (!(object = reiser4_object_open(fs, filename)))
		return -EINVAL;

	/*
	  If --show-items option is specified, we show only items belong to the
	  file. If no, that we show all items whihc lie in the same block as the
	  item belong to the file denoted by @filename.
	*/
	if (PF_ITEMS & flags) {
		aal_stream_t stream;

		aal_stream_init(&stream);
		
		if (reiser4_object_print(object, &stream) == 0)
			debugfs_print_stream(&stream);
		else {
			aal_exception_error("Can't print file %s.",
					    object->name);
			res = -EINVAL;
		}
		
		aal_stream_fini(&stream);
	} else {
		hint.old = 0;
		hint.data = fs;
		hint.flags = flags;

		if (reiser4_object_metadata(object, fprint_process_place, &hint)) {
			aal_exception_error("Can't print object %s metadata.",
					    object->name);
			res = -EINVAL;
		}
	}

	reiser4_object_close(object);
	return res;
}

