/*
  debugfs.c -- program for debugging reiser4 filesystem.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <aux/aux.h>
#include <misc/misc.h>
#include <reiser4/reiser4.h>

enum print_flags {
	PF_SUPER    = 1 << 0,
	PF_JOURNAL  = 1 << 1,
	PF_ALLOC    = 1 << 2,
	PF_OID	    = 1 << 3,
	PF_TREE	    = 1 << 4,
	PF_BLOCK    = 1 << 5,
	PF_FILE     = 1 << 6,
	PF_SITEMS   = 1 << 7
};

typedef enum print_flags print_flags_t;

enum behav_flags {
	BF_FORCE    = 1 << 0,
	BF_QUIET    = 1 << 1,
	BF_TFRAG    = 1 << 2,
	BF_FFRAG    = 1 << 3,
	BF_TSTAT    = 1 << 4,
	BF_DFRAG    = 1 << 5,
	BF_SEACH    = 1 << 6,
	BF_LS       = 1 << 7,
	BF_CAT      = 1 << 8
};

typedef enum behav_flags behav_flags_t;

/* Prints debugfs options */
static void debugfs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);
    
	fprintf(stderr, 
		"Common options:\n"
		"  -?, -h, --help            prints program usage.\n"
		"  -V, --version             prints current version.\n"
		"  -q, --quiet               forces creating filesystem without\n"
		"                            any questions.\n"
		"  -f, --force               makes debugfs to use whole disk, not\n"
		"                            block device or mounted partition.\n"
		"Browsing options:\n"
		"  -l, --ls FILE             browses passed file like standard\n"
		"                            ls program.\n"
		"  -c, --cat FILE            browses passed file like standard\n"
		"                            cat program.\n"
		"Print options:\n"
		"  -t, --print-tree          prints the whole tree.\n"
		"  -j, --print-journal       prints journal.\n"
		"  -s, --print-super         prints the both super blocks.\n"
		"  -b, --print-block-alloc   prints block allocator data.\n"
		"  -o, --print-oid-alloc     prints oid allocator data.\n"
		"  -n, --print-block N       prints block by its number.\n"
		"  -i, --print-file FILE     prints the all file's metadata.\n"
		"  -w, --show-items          forces --print-file show only items\n"
		"                            which are belong to specified file.\n"
		"Measurement options:\n"
		"  -S, --tree-stat           measures some tree characteristics\n"
		"                            (node packing, etc).\n"
		"  -T, --tree-frag           measures tree fragmentation.\n"
		"  -F, --file-frag FILE      measures fragmentation of specified\n"
		"                            file.\n"
		"  -D, --data-frag           measures average files fragmentation.\n"
		"  -p, --show-each           show file fragmentation for each file\n"
		"                            if --data-frag is specified.\n"
		"Plugins options:\n"
		"  -e, --profile PROFILE     profile to be used.\n"
		"  -K, --known-profiles      prints known profiles.\n");
}

/* Initializes used by debugfs exception streams */
static void debugfs_init(void) {
	int ex;

	/* Initializing memory pressure hooks */
	progs_mpressure_init();
	
	/* Setting up exception streams*/
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

static errno_t debugfs_print_buff(void *buff, uint64_t size) {
	int len = size;
	void *ptr = buff;

	while (len > 0) {
		int written;

		if ((written = write(1, ptr, len)) <= 0) {
			if (errno == EINTR)
				continue;
			
			return -1;
		}
		
		ptr += written;
		len -= written;
	}

	return 0;
}

static errno_t debugfs_print_stream(aal_stream_t *stream) {
	return debugfs_print_buff(stream->data, stream->size - 1);
}

/* Callback function used in traverse for opening the node */
static errno_t print_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* block node lies in */
	void *data)		    /* traverse data */
{
	reiser4_tree_t *tree = (reiser4_tree_t *)data;
	aal_device_t *device = tree->fs->device;

	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

static errno_t print_process_node(
	reiser4_node_t *node,	    /* node to be printed */
	void *data)		    /* traverse data */
{	
	aal_stream_t stream;
	aal_stream_init(&stream);

	if (reiser4_node_print(node, &stream))
		goto error_free_stream;

	debugfs_print_stream(&stream);
	aal_stream_fini(&stream);
	
	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

static errno_t debugfs_print_block(reiser4_fs_t *fs, blk_t blk) {
	errno_t res = 0;
	aal_device_t *device;
	reiser4_node_t *node;
	struct traverse_hint hint;

	if (!reiser4_alloc_used_region(fs->alloc, blk, 1)) {
		aal_exception_info("Block %llu is not belong to "
				   "filesystem.", blk);
		return 0;
	}
		
	switch (reiser4_fs_belongs(fs, blk)) {
	case O_SKIPPED:
		aal_exception_info("Block %llu belongs to skipped area.", blk);
		return 0;
	case O_FORMAT:
		aal_exception_info("Sorry, printing format area blocks is not "
				   "implemented yet!");
		return 0;
	case O_JOURNAL:
		aal_exception_info("Sorry, printing journal area blocks is not "
				   "implemented yet!");
		return 0;
	case O_ALLOC:
		aal_exception_info("Sorry, printing block allocator blocks is not "
				   "implemented yet!");
		return 0;
	default:
		break;
	}
	
	aal_exception_disable();
	
	device = fs->device;
	
	if (!(node = reiser4_node_open(device, blk))) {
		aal_exception_enable();
		aal_exception_info("Node %llu is not a formated one.", blk);
		return 0;
	}

	aal_exception_enable();
	
	hint.data = fs->tree;
		
	res = print_process_node(node, &hint);
	reiser4_node_close(node);
	
	return res;
}

static errno_t debugfs_print_tree(reiser4_fs_t *fs) {
	traverse_hint_t hint;
	
	hint.cleanup = 1;
	hint.data = fs->tree;
	
	return reiser4_tree_traverse(fs->tree, &hint, print_open_node, 
			      print_process_node, NULL, NULL, NULL);
}

errno_t debugfs_print_master(reiser4_fs_t *fs) {
	aal_stream_t stream;
	
	aal_assert("umka-1299", fs != NULL, return -1);

	aal_stream_init(&stream);
		
	if (reiser4_master_print(fs->master, &stream))
		return -1;

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	{
		char uuid[37];
		uuid_unparse(reiser4_master_uuid(fs->master), uuid);
		aal_stream_format(&stream, "uuid:\t\t%s\n", uuid);
	}
#endif

	aal_stream_write(&stream, "\n", 1);
	debugfs_print_stream(&stream);
	
	return 0;
}

static errno_t debugfs_print_format(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->format->entity->plugin->format_ops.print) {
		aal_exception_info("Format print method is not implemented.");
		return 0;
	}
    
	aal_stream_init(&stream);
	
	printf("Format super block:\n");
	
	if (reiser4_format_print(fs->format, &stream)) {
		aal_exception_error("Can't print format specific super block.");
		goto error_free_stream;
	}
    
	aal_stream_write(&stream, "\n", 1);
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

static errno_t debugfs_print_oid(reiser4_fs_t *fs) {
	aal_stream_t stream;
    
	if (!fs->oid->entity->plugin->oid_ops.print) {
		aal_exception_info("Oid allocator print method is not implemented.");
		return 0;
	}

	aal_stream_init(&stream);
    
	printf("Oid allocator:\n");
	if (fs->oid->entity->plugin->oid_ops.print(fs->oid->entity,
						   &stream, 0))
	{
		aal_exception_error("Can't print oid allocator.");
		goto error_free_stream;;
	}

	aal_stream_write(&stream, "\n", 1);
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

static errno_t debugfs_print_alloc(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, block allocator print "
			    "is not implemented yet!");
	return 0;
}
   
static errno_t debugfs_print_journal(reiser4_fs_t *fs) {
	aal_exception_error("Sorry, journal print is not implemented yet!");
	return 0;
}

struct tfrag_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	blk_t curr;
	uint16_t level;
	count_t total, bad;
};

typedef struct tfrag_hint tfrag_hint_t;

static errno_t tfrag_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* blk node lies in */
	void *data)		    /* traverse hint */
{	
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;
	aal_device_t *device = frag_hint->tree->fs->device;

	aal_assert("umka-1556", frag_hint->level > 0, return -1);
	
	*node = NULL;
	
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;
	
	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

static errno_t tfrag_process_node(
	reiser4_node_t *node,	   /* node to be estimated */
	void *data)	           /* user-specified data */
{
	reiser4_pos_t pos;
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;

	if (frag_hint->level <= LEAF_LEVEL)
		return 0;

	aal_gauge_update(frag_hint->gauge, 0);
		
	pos.unit = ~0ul;
	
	for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
		int64_t delta;
		reiser4_coord_t coord;
		reiser4_ptr_hint_t ptr;

		if (reiser4_coord_open(&coord, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		if (reiser4_item_extent(&coord)) {
			for (pos.unit = 0; pos.unit < reiser4_item_units(&coord); pos.unit++) {
				
				plugin_call(continue, coord.item.plugin->item_ops,
					    fetch, &coord.item, &ptr, pos.unit, 1);

				if (ptr.ptr == 0)
					continue;
				
				delta = frag_hint->curr - ptr.ptr;
				
				if (labs(delta) > 1)
					frag_hint->bad++;
				
				frag_hint->curr = ptr.ptr + ptr.width;
				frag_hint->total += ptr.width;
			}
		} else {
			plugin_call(continue, coord.item.plugin->item_ops,
				    fetch, &coord.item, &ptr, pos.unit, 1);

			delta = frag_hint->curr - ptr.ptr;

			if (labs(delta) > 1)
				frag_hint->bad++;

			frag_hint->total++;
			frag_hint->curr = ptr.ptr;
		}
	}
	
	return 0;
}

static errno_t tfrag_setup_node(reiser4_coord_t *coord, void *data) {
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;

	frag_hint->level--;
	return 0;
}

static errno_t tfrag_update_node(reiser4_coord_t *coord, void *data) {
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;

	frag_hint->level++;
	return 0;
}

static errno_t debugfs_tree_frag(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	reiser4_node_t *root;
	tfrag_hint_t frag_hint;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Tree fragmentation",
				       progs_gauge_handler, NULL)))
		return -1;
	
	root = fs->tree->root;

	frag_hint.bad = 0;
	frag_hint.total = 0;
	frag_hint.gauge = gauge;
	frag_hint.tree = fs->tree;
	frag_hint.curr = root->blk;
	frag_hint.level = reiser4_node_level(root);

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.cleanup = 1;
	hint.data = (void *)&frag_hint;

	aal_gauge_start(gauge);
	
	if (reiser4_tree_traverse(fs->tree, &hint, tfrag_open_node, tfrag_process_node,
				  tfrag_setup_node, tfrag_update_node, NULL))
		return -1;

	aal_gauge_free(gauge);
	
	printf("%.5f\n", frag_hint.total > 0 ?
	       (double)frag_hint.bad / frag_hint.total : 0);
	
	return 0;
};

struct tree_stat_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	double formatted_used;
	double leaves_used;
	double internals_used;

	count_t nodes;
	count_t leaves;
	count_t internals;
	count_t formatted;
};

typedef struct tree_stat_hint tree_stat_hint_t;

static errno_t stat_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* block node lies in */
	void *data)		    /* traverse data */
{
	tree_stat_hint_t *stat_hint = (tree_stat_hint_t *)data;
	aal_device_t *device = stat_hint->tree->fs->device;

	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

static errno_t stat_process_node(
	reiser4_node_t *node,	    /* node to be inspected */
	void *data)		    /* traverse data */
{
	uint8_t level;
	aal_device_t *device;
	uint32_t leaves_used;
	uint32_t formatted_used;
	uint32_t internals_used;

	tree_stat_hint_t *stat_hint =
		(tree_stat_hint_t *)data;

	level = reiser4_node_level(node);

	if (stat_hint->formatted % 128 == 0)
		aal_gauge_update(stat_hint->gauge, 0);

	device = node->device;
	formatted_used = aal_device_get_bs(device) - reiser4_node_space(node);

	stat_hint->formatted_used = formatted_used +
		(stat_hint->formatted_used * stat_hint->formatted);

	stat_hint->formatted_used /= (stat_hint->formatted + 1);

	if (level > LEAF_LEVEL) {
		uint32_t units;
		item_entity_t *item;
		reiser4_coord_t coord;
		reiser4_pos_t pos = {~0ul, ~0ul};
		
		internals_used = aal_device_get_bs(device) -
			reiser4_node_space(node);
		
		stat_hint->internals_used = internals_used +
			(stat_hint->internals_used * stat_hint->internals);

		stat_hint->internals_used /= (stat_hint->internals + 1);

		for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
			reiser4_coord_t coord;

			if (reiser4_coord_open(&coord, node, &pos)) {
				aal_exception_error("Can't open item %u in node %llu.", 
						    pos.item, node->blk);
				return -1;
			}

			if (!reiser4_item_extent(&coord))
				continue;

			item = &coord.item;
				
			units = plugin_call(return -1, item->plugin->item_ops,
					    units, item);

			for (pos.unit = 0; pos.unit < units; pos.unit++) {
				reiser4_ptr_hint_t ptr;
				
				plugin_call(return -1, item->plugin->item_ops, fetch, item, 
					    &ptr, pos.unit, 1);

				stat_hint->nodes += ptr.width;
			}
		}
	} else {
		leaves_used = aal_device_get_bs(device) -
			reiser4_node_space(node);

		stat_hint->leaves_used = leaves_used +
			(stat_hint->leaves_used * stat_hint->leaves);
		
		stat_hint->leaves_used /= (stat_hint->leaves + 1);
	}
	
	if (level > LEAF_LEVEL)
		stat_hint->internals++;
	else
		stat_hint->leaves++;
		
	stat_hint->formatted++;
	stat_hint->nodes++;
	
	return 0;
}

static errno_t debugfs_tree_stat(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	tree_stat_hint_t stat_hint;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Tree statistics",
				       progs_gauge_handler, NULL)))
		return -1;
	
	aal_memset(&stat_hint, 0, sizeof(stat_hint));

	stat_hint.tree = fs->tree;
	stat_hint.gauge = gauge;

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.cleanup = 1;
	hint.data = (void *)&stat_hint;

	aal_gauge_start(gauge);
	
	if (reiser4_tree_traverse(fs->tree, &hint, stat_open_node,
				  stat_process_node, NULL, NULL, NULL))
		return -1;

	aal_gauge_free(gauge);
	progs_wipe_line(stdout);

	printf("Formatted packing:\t%.2f\n", stat_hint.formatted_used);
	printf("Leaves packing:\t\t%.2f\n", stat_hint.leaves_used);
	printf("Internals packing:\t%.2f\n\n", stat_hint.internals_used);

	printf("Total nodes:\t\t%llu\n", stat_hint.nodes);
	printf("Formatted nodes:\t%llu\n", stat_hint.formatted);
	printf("Leaf nodes:\t\t%llu\n", stat_hint.leaves);
	printf("Internal nodes:\t\t%llu\n", stat_hint.internals);
	
	return 0;
}

struct ffrag_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	blk_t curr;

	uint32_t flags;
	
	count_t fs_total, fs_bad;
	count_t fl_total, fl_bad;
	uint16_t level;
};

typedef struct ffrag_hint ffrag_hint_t;

static errno_t ffrag_process_blk(
	object_entity_t *entity,   /* file to be inspected */
	blk_t blk,                 /* next file block */
	void *data)                /* user-specified data */
{
	int64_t delta;
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;

	if (frag_hint->curr == 0) {
		frag_hint->curr = blk;
		return 0;
	}
	
	delta = frag_hint->curr - blk;

	if (labs(delta) > 1) {
		frag_hint->fs_bad++;
		frag_hint->fl_bad++;
	}
	
	frag_hint->fs_total++;
	frag_hint->fl_total++;
	frag_hint->curr = blk;

	return 0;
}

static errno_t debugfs_file_frag(reiser4_fs_t *fs, char *filename) {
	aal_gauge_t *gauge;
	reiser4_file_t *file;
	ffrag_hint_t frag_hint;

	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "",
				       progs_gauge_handler, NULL)))
		goto error_free_file;
	
	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	frag_hint.tree = fs->tree;
	frag_hint.gauge = gauge;

	aal_gauge_rename(gauge, "Fragmentation for %s is", filename);
	aal_gauge_start(gauge);
	
	if (reiser4_file_layout(file, ffrag_process_blk, &frag_hint)) {
		aal_exception_error("Can't enumerate data blocks occupied by %s",
				    filename);
		goto error_free_gauge;
	}
	
	aal_gauge_free(gauge);

	printf("%.5f\n", frag_hint.fl_total > 0 ?
	       (double)frag_hint.fl_bad / frag_hint.fl_total : 0);

	reiser4_file_close(file);
	
	return 0;

 error_free_gauge:
	aal_gauge_free(gauge);
 error_free_file:
	reiser4_file_close(file);
	return -1;
}

static errno_t dfrag_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* block node lies in */
	void *data)		    /* traverse data */
{
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;
	aal_device_t *device = frag_hint->tree->fs->device;

	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

static errno_t dfrag_process_node(
	reiser4_node_t *node,       /* node to be inspected */
	void *data)                 /* traverse hint */
{
	reiser4_pos_t pos;
	static int bogus = 0;
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;

	if (frag_hint->level > LEAF_LEVEL)
		return 0;
	
	pos.unit = ~0ul;

	for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
		reiser4_file_t *file;
		reiser4_coord_t coord;

		if (reiser4_coord_open(&coord, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		if (!reiser4_item_statdata(&coord))
			continue;

		if (!(file = reiser4_file_begin(frag_hint->tree->fs, &coord)))
			continue;

		frag_hint->curr = 0;
		frag_hint->fl_bad = 0;
		frag_hint->fl_total = 0;

		if (bogus++ % 16 == 0)
			aal_gauge_update(frag_hint->gauge, 0);

		bogus %= 16;
	
		if (reiser4_file_layout(file, ffrag_process_blk, data)) {
			aal_exception_error("Can't enumerate data blocks occupied by %s",
					    file->name);
			
			reiser4_file_close(file);
			continue;
		}

		if (frag_hint->flags & BF_SEACH) {
			double factor = frag_hint->fl_total > 0 ?
				(double)frag_hint->fl_bad / frag_hint->fl_total : 0;
			
			aal_exception_info("Fragmentation for %s: %.5f",
					   file->name, factor);
		}
		
		reiser4_file_close(file);
	}
	
	return 0;
}

static errno_t dfrag_setup_node(reiser4_coord_t *coord, void *data) {
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;
    
	frag_hint->level--;
	return 0;
}

static errno_t dfrag_update_node(reiser4_coord_t *coord, void *data) {
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;

	frag_hint->level++;
	return 0;
}

static errno_t debugfs_data_frag(reiser4_fs_t *fs, uint32_t flags) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	ffrag_hint_t frag_hint;

	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Data fragmentation",
				       progs_gauge_handler, NULL)))
		return -1;
	
	aal_memset(&frag_hint, 0, sizeof(frag_hint));

	frag_hint.tree = fs->tree;
	frag_hint.gauge = gauge;
	frag_hint.flags = flags;
	frag_hint.level = reiser4_node_level(fs->tree->root);

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.cleanup = 1;
	hint.data = (void *)&frag_hint;

	aal_gauge_start(gauge);
	
	if (reiser4_tree_traverse(fs->tree, &hint, dfrag_open_node,
				  dfrag_process_node, dfrag_setup_node, 
				  dfrag_update_node, NULL))
		return -1;

	aal_gauge_free(gauge);

	if (frag_hint.flags & BF_SEACH)
		printf("Data fragmentation is: ");
	
	printf("%.5f\n", frag_hint.fs_total > 0 ?
	       (double)frag_hint.fs_bad / frag_hint.fs_total : 0);
	
	return 0;
}

static errno_t debugfs_file_cat(reiser4_file_t *file) {
	int32_t read;
	char buff[4096];
	
	if (reiser4_file_reset(file)) {
		aal_exception_error("Can't reset file %s.", file->name);
		return -1;
	}
	
	while (1) {
		aal_memset(buff, 0, sizeof(buff));

		if (!(read = reiser4_file_read(file, buff, sizeof(buff))))
			break;

		debugfs_print_buff(buff, read);
	}

	return 0;
}

static errno_t debugfs_file_ls(reiser4_file_t *file) {
	reiser4_entry_hint_t entry;
	
	if (reiser4_file_reset(file)) {
		aal_exception_error("Can't reset file %s.", file->name);
		return -1;
	}
	
	while (reiser4_file_read(file, &entry, 1)) {
		aal_stream_t stream;
		aal_stream_init(&stream);
		
		reiser4_key_print(&entry.object, &stream);
		aal_stream_format(&stream, " %s\n", entry.name);
		debugfs_print_stream(&stream);
		
		aal_stream_fini(&stream);
	}

	printf("\n");
	
	return 0;
}

static errno_t debugfs_browse(reiser4_fs_t *fs, char *filename) {
	errno_t res = 0;
	reiser4_file_t *file;
	
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	if (file->entity->plugin->h.group == REGULAR_FILE)
		res = debugfs_file_cat(file);
	else if (file->entity->plugin->h.group == DIRTORY_FILE)
		res = debugfs_file_ls(file);
	else {
		aal_exception_info("Sorry, browsing special files and symlinks "
				   "is not implemented yet.");
	}
	
	reiser4_file_close(file);
	return res;
}

struct fprint_hint {
	blk_t old;
	void *data;
	uint32_t flags;
};

typedef struct fprint_hint fprint_hint_t;

static errno_t fprint_process_place(
	object_entity_t *entity,   /* file to be inspected */
	reiser4_place_t *place,    /* next file block */
	void *data)                /* user-specified data */
{
	fprint_hint_t *hint = (fprint_hint_t *)data;
	reiser4_coord_t *coord = (reiser4_coord_t *)place;

	if (coord->node->blk == hint->old)
		return 0;

	hint->old = coord->node->blk;
	
	if (print_process_node(coord->node, NULL)) {
		aal_exception_error("Can't print node %llu.",
				    hint->old);
		return -1;
	}

	return 0;
}

static errno_t debugfs_print_file(reiser4_fs_t *fs,
				  char *filename,
				  uint32_t flags)
{
	errno_t res = 0;
	fprint_hint_t hint;
	reiser4_file_t *file;
	
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	if (PF_SITEMS & flags) {
		aal_stream_t stream;

		aal_stream_init(&stream);
		
		if (reiser4_file_print(file, &stream) == 0)
			debugfs_print_stream(&stream);
		else {
			aal_exception_error("Can't print file %s.",
					    file->name);
			res = -1;
		}
		
		aal_stream_fini(&stream);
	} else {
		hint.old = 0;
		hint.data = fs;
		hint.flags = flags;

		if (reiser4_file_metadata(file, fprint_process_place, &hint)) {
			aal_exception_error("Can't print file %s metadata.",
					    file->name);
			res = -1;
		}
	}

	reiser4_file_close(file);
	return res;
}

int main(int argc, char *argv[]) {
	int c;
	struct stat st;
	char *host_dev;
	uint32_t print_flags = 0;
	uint32_t behav_flags = 0;
    
	char *ls_filename = NULL;
	char *cat_filename = NULL;
	char *frag_filename = NULL;
	char *print_filename = NULL;
	
	char *profile_label = "smart40";
    
	reiser4_fs_t *fs;
	aal_device_t *device;
	reiser4_profile_t *profile;

	blk_t blocknr = 0;
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"profile", required_argument, NULL, 'e'},
		{"force", no_argument, NULL, 'f'},
		{"ls", required_argument, NULL, 'l'},
		{"cat", required_argument, NULL, 'c'},
		{"print-tree", no_argument, NULL, 't'},
		{"print-journal", no_argument, NULL, 'j'},
		{"print-super", no_argument, NULL, 's'},
		{"print-block-alloc", no_argument, NULL, 'b'},
		{"print-oid-alloc", no_argument, NULL, 'o'},
		{"print-block", required_argument, NULL, 'n'},
		{"print-file", required_argument, NULL, 'i'},
		{"show-items", no_argument, NULL, 'w'},
		{"tree-stat", no_argument, NULL, 'S'},
		{"tree-frag", no_argument, NULL, 'T'},
		{"file-frag", required_argument, NULL, 'F'},
		{"data-frag", no_argument, NULL, 'D'},
		{"show-each", no_argument, NULL, 'p'},
		{"known-profiles", no_argument, NULL, 'K'},
		{"quiet", no_argument, NULL, 'q'},
		{0, 0, 0, 0}
	};

	debugfs_init();

	if (argc < 2) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	/* Parsing parameters */    
	while ((c = getopt_long(argc, argv, "hVe:qfKtbojTDpSF:c:l:n:i:w",
				long_options, (int *)0)) != EOF) 
	{
		switch (c) {
		case 'h':
			debugfs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			progs_print_banner(argv[0]);
			return NO_ERROR;
		case 'e':
			profile_label = optarg;
			break;
		case 'o':
			print_flags |= PF_OID;
			break;
		case 'b':
			print_flags |= PF_ALLOC;
			break;
		case 's':
			print_flags |= PF_SUPER;
			break;
		case 'j':
			print_flags |= PF_JOURNAL;
			break;
		case 't':
			print_flags |= PF_TREE;
			break;
		case 'n': {
			int error;
			
			print_flags |= PF_BLOCK;
			
			if (!(blocknr = aux_strtol(optarg, &error)) && error) {
				aal_exception_error("Invalid block number (%s).", optarg);
				return USER_ERROR;
			}
			
			break;
		}
		case 'i':
			print_flags |= PF_FILE;
			print_filename = optarg;
			break;
		case 'w':
			print_flags |= PF_SITEMS;
			break;
		case 'S':
			behav_flags |= BF_TSTAT;
			break;
		case 'T':
			behav_flags |= BF_TFRAG;
			break;
		case 'D':
			behav_flags |= BF_DFRAG;
			break;
		case 'p':
			behav_flags |= BF_SEACH;
			break;
		case 'c':
			behav_flags |= BF_CAT;
			cat_filename = optarg;
			break;
		case 'l':
			behav_flags |= BF_LS;
			ls_filename = optarg;
			break;
		case 'F':
			behav_flags |= BF_FFRAG;
			frag_filename = optarg;
			break;
		case 'f':
			behav_flags |= BF_FORCE;
			break;
		case 'q':
			behav_flags |= BF_QUIET;
			break;
		case 'K':
			progs_print_banner(argv[0]);
			progs_profile_list();
			return NO_ERROR;
		case '?':
			debugfs_print_usage(argv[0]);
			return USER_ERROR;
		}
	}
    
	if (optind >= argc) {
		debugfs_print_usage(argv[0]);
		return USER_ERROR;
	}
    
	progs_print_banner(argv[0]);
    
	/* Initializing passed profile */
	if (!(profile = progs_profile_find(profile_label))) {
		aal_exception_error("Can't find profile by its label %s.", 
				    profile_label);
		goto error;
	}
    
	if (libreiser4_init()) {
		aal_exception_error("Can't initialize libreiser4.");
		goto error;
	}

	host_dev = argv[optind];
    
	if (stat(host_dev, &st) == -1) {
		aal_exception_error("Can't stat %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
	
	/* 
	   Checking is passed device is a block device. If so, we check also is
	   it whole drive or just a partition. If the device is not a block
	   device, then we emmit exception and propose user to use -f flag to
	   force.
	*/
	if (!S_ISBLK(st.st_mode)) {
		if (!(behav_flags & BF_FORCE)) {
			aal_exception_error("Device %s is not block device. "
					    "Use -f to force over.", host_dev);
			goto error_free_libreiser4;
		}
	} else {
		if (((IDE_DISK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 64 == 0) ||
		     (SCSI_BLK_MAJOR(MAJOR(st.st_rdev)) && MINOR(st.st_rdev) % 16 == 0)) &&
		    (!(behav_flags & BF_FORCE)))
		{
			aal_exception_error("Device %s is an entire harddrive, not "
					    "just one partition.", host_dev);
			goto error_free_libreiser4;
		}
	}
   
	/* Checking if passed partition is mounted */
	if (progs_dev_mounted(host_dev, NULL) && !(behav_flags & BF_FORCE)) {
		aal_exception_error("Device %s is mounted at the moment. "
				    "Use -f to force over.", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening device */
	if (!(device = aal_device_open(&file_ops, host_dev, DEFAULT_BLOCKSIZE, O_RDONLY))) {
		aal_exception_error("Can't open %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}
    
	if (!(fs = reiser4_fs_open(device, device))) {
		aal_exception_error("Can't open reiser4 on %s", host_dev);
		goto error_free_libreiser4;
	}
    
	if (!aal_pow_of_two(print_flags) && !(behav_flags & BF_QUIET) &&
	    !(print_flags & PF_SITEMS))
	{
		if (aal_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_YESNO,
					"Few print options has been detected. "
					"Continue?") == EXCEPTION_NO)
			goto error_free_fs;
	}

	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;
		
	if (!(behav_flags & BF_DFRAG) && (behav_flags & BF_SEACH)) {
		aal_exception_warn("Option --show-each is only active if "
				   "--data-frag is specified.");
	}

	if (!(print_flags & PF_FILE) && (print_flags & PF_SITEMS)) {
		aal_exception_warn("Option --show-items is only active if "
				   "--print-file is specified.");
	}

	if (behav_flags & BF_TFRAG || behav_flags & BF_DFRAG ||
	    behav_flags & BF_FFRAG || behav_flags & BF_TSTAT)
	{
		if (behav_flags & BF_QUIET ||
		    aal_exception_yesno("This operation may take a long time. "
					"Continue?") == EXCEPTION_YES)
		{
			if (behav_flags & BF_TFRAG) {
				if (debugfs_tree_frag(fs))
					goto error_free_fs;
			}

			if (behav_flags & BF_DFRAG) {
				if (debugfs_data_frag(fs, behav_flags))
					goto error_free_fs;
			}

			if (behav_flags & BF_FFRAG) {
				if (debugfs_file_frag(fs, frag_filename))
					goto error_free_fs;
			}
	
			if (behav_flags & BF_TSTAT) {
				if (debugfs_tree_stat(fs))
					goto error_free_fs;
			}
		}
	}
	
	if ((behav_flags & BF_LS)) {
		if (debugfs_browse(fs, ls_filename))
			goto error_free_fs;
	}
	
	if ((behav_flags & BF_CAT)) {
		if (debugfs_browse(fs, cat_filename))
			goto error_free_fs;
	}
	
	if (print_flags & PF_SUPER) {
		if (debugfs_print_master(fs))
			goto error_free_fs;
	
		if (debugfs_print_format(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_OID) {
		if (debugfs_print_oid(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_ALLOC) {
		if (debugfs_print_alloc(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_JOURNAL) {
		if (debugfs_print_journal(fs))
			goto error_free_fs;
	}
    
	if (print_flags & PF_TREE) {
		if (debugfs_print_tree(fs))
			goto error_free_fs;
	}

	if (print_flags & PF_BLOCK) {
		if (debugfs_print_block(fs, blocknr))
			goto error_free_fs;
	}
    
	if (print_flags & PF_FILE) {
		if (debugfs_print_file(fs, print_filename, print_flags))
			goto error_free_fs;
	}
    
	/* Deinitializing filesystem instance and device instance */
	reiser4_fs_close(fs);
	aal_device_close(device);
    
	/* 
	   Deinitializing libreiser4. At the moment only plugins are unloading 
	   durrign this.
	*/
	libreiser4_done();
    
	return NO_ERROR;

 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_done();
 error:
	return OPER_ERROR;
}

