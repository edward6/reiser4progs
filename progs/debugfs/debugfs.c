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
#include <stdlib.h>
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

	/* Setting up exception streams */
	for (ex = 0; ex < aal_log2(EXCEPTION_LAST); ex++)
		progs_exception_set_stream(ex, stderr);
}

/*
  Prints passed @buff into stdout. The special print function is needed because
  we can't just put 4k buffer into stdout.
*/
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

static errno_t debugfs_attach_handler(reiser4_tree_t *tree,
				      reiser4_coord_t *coord,
				      reiser4_node_t *node,
				      void *data)
{
	if (tree->lru) {
		if (progs_mpressure_detect())
			return aal_lru_adjust(tree->lru);
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

/* Prints passed node */
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

/* Prints block denoted as blk */
static errno_t debugfs_print_block(reiser4_fs_t *fs, blk_t blk) {
	errno_t res = 0;
	aal_device_t *device;
	reiser4_node_t *node;
	struct traverse_hint hint;

	/* Check if @blk is a filesystem block at all */
	if (!reiser4_alloc_used_region(fs->alloc, blk, 1)) {
		aal_exception_info("Block %llu is not belong to "
				   "filesystem.", blk);
		return 0;
	}

	/* Determining whata object this block belong to */
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

	/*
	  If passed @blk points to a formatted node then open it and print
	  using print_process_node listed abowe.
	*/
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

/* Makes traverse though the whole tree and prints all nodes */
static errno_t debugfs_print_tree(reiser4_fs_t *fs) {
	traverse_hint_t hint;
	
	hint.cleanup = 1;
	hint.data = fs->tree;
	
	return reiser4_tree_traverse(fs->tree, &hint, print_open_node, 
			      print_process_node, NULL, NULL, NULL);
}

/* Prints master super block */
errno_t debugfs_print_master(reiser4_fs_t *fs) {
	aal_stream_t stream;
	
	aal_assert("umka-1299", fs != NULL, return -1);

	aal_stream_init(&stream);
		
	if (reiser4_master_print(fs->master, &stream))
		return -1;

	/*
	  If reiser4progs supports uuid (if it was found durring building), then
	  it will also print uuid stored in master super block.
	*/
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	{
		char uuid[37];
		uuid_unparse(reiser4_master_uuid(fs->master), uuid);
		aal_stream_format(&stream, "uuid:\t\t%s\n", uuid);
	}
#endif

	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);
	
	aal_stream_fini(&stream);
	return 0;
}

/* Prints format-specific super block */
static errno_t debugfs_print_format(reiser4_fs_t *fs) {
	aal_stream_t stream;

	if (!fs->format->entity->plugin->format_ops.print) {
		aal_exception_info("Format print method is not implemented.");
		return 0;
	}
    
	aal_stream_init(&stream);
	
	if (reiser4_format_print(fs->format, &stream)) {
		aal_exception_error("Can't print format specific super block.");
		goto error_free_stream;
	}
    
	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

/* Prints oid allocator */
static errno_t debugfs_print_oid(reiser4_fs_t *fs) {
	aal_stream_t stream;
    
	if (!fs->oid->entity->plugin->oid_ops.print) {
		aal_exception_info("Oid allocator print method is not implemented.");
		return 0;
	}

	aal_stream_init(&stream);

	if (reiser4_oid_print(fs->oid, &stream)) {
		aal_exception_error("Can't print oid allocator.");
		goto error_free_stream;;
	}

	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

/* Prints block allocator */
static errno_t debugfs_print_alloc(reiser4_fs_t *fs) {
	aal_stream_t stream;

	aal_stream_init(&stream);
    
	if (reiser4_alloc_print(fs->alloc, &stream)) {
		aal_exception_error("Can't print block allocator.");
		goto error_free_stream;;
	}

	aal_stream_format(&stream, "\n");
	debugfs_print_stream(&stream);

	aal_stream_fini(&stream);
    	return 0;
	
 error_free_stream:
	aal_stream_fini(&stream);
	return -1;
}

/* Prints journal */
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

/* Open node callback for calculating the tree fragmentation */
static errno_t tfrag_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* blk node lies in */
	void *data)		    /* traverse hint */
{	
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;
	aal_device_t *device = frag_hint->tree->fs->device;

	aal_assert("umka-1556", frag_hint->level > 0, return -1);
	
	*node = NULL;

	/* As we do not need traverse leaf level at all, we going out here */
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;
	
	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

static errno_t tfrag_process_item(
	item_entity_t *item,        /* item we traverse now */
	uint64_t start,             /* start item block item */
	uint64_t end,               /* end item block */
	void *data)
{
	uint32_t blk;
	int64_t delta;
	tfrag_hint_t *hint;
	
	hint = (tfrag_hint_t *)data;
	
	if (start == 0)
		return 0;

	for (blk = start; blk < end; blk++) {
		delta = hint->curr - blk;
				
		if (labs(delta) > 1)
			hint->bad++;
				
		hint->total++;
		hint->curr = blk;
	}

	return 0;
}

/*
  Traverse passed leaf @node and calculate fragmentation for it. The results are
  stored in frag_hint structure. This function is called from the tree traversal
  routine for each internal node. See bellow for details.
*/
static errno_t tfrag_process_node(
	reiser4_node_t *node,	   /* node to be estimated */
	void *data)	           /* user-specified data */
{
	rpos_t pos;
	tfrag_hint_t *frag_hint;

	frag_hint = (tfrag_hint_t *)data;
	
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;

	aal_gauge_update(frag_hint->gauge, 0);
		
	pos.unit = ~0ul;

	/* Loop though the node items */
	for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
		item_entity_t *item;
		reiser4_coord_t coord;

		/* Initializing item at @coord */
		if (reiser4_coord_open(&coord, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		item = &coord.item;
		
		/*
		  Checking and calling item's layout method with function
		  tfrag_process_item as a function for handling one block the
		  item points to.
		*/
		if (!item->plugin->item_ops.layout) {
			aal_exception_warn("Item %u in node %llu has not "
					   "\"layout\" method implemented. "
					   "The result will not be reliable.",
					   pos.item, node->blk);
			continue;
		}

		item->plugin->item_ops.layout(item, tfrag_process_item, data);
	}
	
	return 0;
}

/*
  Traverse callbacks for keeping track the current level we are on. They are
  needed for make dependence from the node's "level" field lesses in our
  code. That is baceuse that filed is counting as optional one and probably will
  be eliminated soon.
*/
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

/*
  Entry point for calculating tree fragmentation. It zeroes out all counters in
  structure which wiil be passed to actual routines and calls tree_traverse
  function  with couple of callbacks for handling all traverse cases (open
  node, traverse node, etc). Actual statistics collecting is performed in the
  passed callbacks and subcallbacks (for item traversing).
*/
static errno_t debugfs_tree_frag(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	reiser4_node_t *root;
	tfrag_hint_t frag_hint;

	/*
	  Initializing gauge, because it is a long process and user should be
	  informated what the stage of the process is going on in the moment.
	*/
	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "Tree fragmentation",
				       progs_gauge_handler, NULL)))
		return -1;
	
	root = fs->tree->root;

	/* Preparing serve structure, statistics will be stored in  */
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

	/* Calling tree traversal */
	if (reiser4_tree_traverse(fs->tree, &hint, tfrag_open_node, tfrag_process_node,
				  tfrag_setup_node, tfrag_update_node, NULL))
		return -1;

	aal_gauge_free(gauge);

	/* Printing the result */
	printf("%.5f\n", frag_hint.total > 0 ?
	       (double)frag_hint.bad / frag_hint.total : 0);
	
	return 0;
};

struct tstat_hint {
	reiser4_tree_t *tree;
	aal_gauge_t *gauge;

	double formatted_used;
	double leaves_used;
	double internals_used;

	count_t nodes;
	count_t leaves;
	count_t twigs;
	count_t internals;
	count_t formatted;
};

typedef struct tstat_hint tstat_hint_t;

/* Open node for tree staticstics process */
static errno_t stat_open_node(
	reiser4_node_t **node,      /* node to be opened */
	blk_t blk,                  /* block node lies in */
	void *data)		    /* traverse data */
{
	tstat_hint_t *stat_hint = (tstat_hint_t *)data;
	aal_device_t *device = stat_hint->tree->fs->device;

	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

/* Process one block belong to the item (extent or nodeptr) */
static errno_t stat_process_item(
	item_entity_t *item,        /* item we traverse now */
	uint64_t start,             /* start item block*/
	uint64_t end,               /* end item block */
	void *data)
{
	tstat_hint_t *stat_hint = (tstat_hint_t *)data;
	stat_hint->nodes += end - start;

	return 0;
}

/* Processing one formatted node */
static errno_t stat_process_node(
	reiser4_node_t *node,	    /* node to be inspected */
	void *data)		    /* traverse data */
{
	uint8_t level;
	aal_device_t *device;
	uint32_t leaves_used;
	uint32_t formatted_used;
	uint32_t internals_used;

	tstat_hint_t *stat_hint = (tstat_hint_t *)data;

	level = reiser4_node_level(node);

	if (stat_hint->formatted % 128 == 0)
		aal_gauge_update(stat_hint->gauge, 0);

	device = node->device;
	
	formatted_used = aal_device_get_bs(device) -
		reiser4_node_space(node);

	stat_hint->formatted_used = formatted_used +
		(stat_hint->formatted_used * stat_hint->formatted);

	stat_hint->formatted_used /= (stat_hint->formatted + 1);

	/*
	  If we are on the level higher taht leaf level, we traverse extents on
	  it. Otherwise we just update stat structure.
	*/
	if (level > LEAF_LEVEL) {
		rpos_t pos = {~0ul, ~0ul};
		
		internals_used = aal_device_get_bs(device) -
			reiser4_node_space(node);
		
		stat_hint->internals_used = internals_used +
			(stat_hint->internals_used * stat_hint->internals);

		stat_hint->internals_used /= (stat_hint->internals + 1);

		for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
			item_entity_t *item;
			reiser4_coord_t coord;
			
			if (reiser4_coord_open(&coord, node, &pos)) {
				aal_exception_error("Can't open item %u in node %llu.", 
						    pos.item, node->blk);
				return -1;
			}

			item = &coord.item;
			
			if (!reiser4_item_extent(&coord))
				continue;

			if (!item->plugin->item_ops.layout)
				continue;

			item->plugin->item_ops.layout(item, stat_process_item, data);
		}
	} else {
		leaves_used = aal_device_get_bs(device) -
			reiser4_node_space(node);

		stat_hint->leaves_used = leaves_used +
			(stat_hint->leaves_used * stat_hint->leaves);
		
		stat_hint->leaves_used /= (stat_hint->leaves + 1);
	}
	
	stat_hint->leaves += (level == LEAF_LEVEL);
	stat_hint->twigs += (level == TWIG_LEVEL);
	stat_hint->internals += (level > LEAF_LEVEL);

	stat_hint->nodes++;
	stat_hint->formatted++;

	return 0;
}

/* Ebtry point function for calculating tree statistics */
static errno_t debugfs_tree_stat(reiser4_fs_t *fs) {
	aal_gauge_t *gauge;
	traverse_hint_t hint;
	tstat_hint_t stat_hint;

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

	/* Printing results */
	printf("Formatted packing:\t%.2f\n", stat_hint.formatted_used);
	printf("Leaves packing:\t\t%.2f\n", stat_hint.leaves_used);
	printf("Internals packing:\t%.2f\n\n", stat_hint.internals_used);

	printf("Total nodes:\t\t%llu\n", stat_hint.nodes);
	printf("Formatted nodes:\t%llu\n", stat_hint.formatted);
	printf("Leaf nodes:\t\t%llu\n", stat_hint.leaves);
	printf("Twig nodes:\t\t%llu\n", stat_hint.twigs);
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

/*
  Callback function for processing one block belong to the fiel we are
  traversing.
*/
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

/* Calculates the passed file fragmentation */
static errno_t debugfs_file_frag(reiser4_fs_t *fs, char *filename) {
	aal_gauge_t *gauge;
	reiser4_file_t *file;
	ffrag_hint_t frag_hint;

	/* Opens file by its name */
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	/* Create a gauge which will show the progress */
	if (!(gauge = aal_gauge_create(GAUGE_INDICATOR, "",
				       progs_gauge_handler, NULL)))
		goto error_free_file;

	/* Initializing serve structures */
	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	frag_hint.tree = fs->tree;
	frag_hint.gauge = gauge;

	aal_gauge_rename(gauge, "Fragmentation for %s is", filename);
	aal_gauge_start(gauge);

	/*
	  Calling file layout function, wich will call ffrag_process_blk
	  fucntion on each block belong to the file denoted by @filename. Actual
	  data file fragmentation will be calculated on are gathering in that
	  function.
	*/
	if (reiser4_file_layout(file, ffrag_process_blk, &frag_hint)) {
		aal_exception_error("Can't enumerate data blocks occupied by %s",
				    filename);
		goto error_free_gauge;
	}
	
	aal_gauge_free(gauge);
	reiser4_file_close(file);

	/* Showing the results */
	printf("%.5f\n", frag_hint.fl_total > 0 ?
	       (double)frag_hint.fl_bad / frag_hint.fl_total : 0);
	
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

/*
  Processes leaf node in order to find all the stat data items which denote
  corresponding files and calculate file fragmentation for each of them.
*/
static errno_t dfrag_process_node(
	reiser4_node_t *node,       /* node to be inspected */
	void *data)                 /* traverse hint */
{
	rpos_t pos;
	static int bogus = 0;
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;

	if (frag_hint->level > LEAF_LEVEL)
		return 0;
	
	pos.unit = ~0ul;

	/* The loop though the all items in current node */
	for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
		reiser4_file_t *file;
		reiser4_coord_t coord;

		/* Initialiing the item at @coord */
		if (reiser4_coord_open(&coord, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		/*
		  If the item is not a stat data item, we getting to the next
		  circle of the loop, because we are intersted only in the stat
		  data items.
		*/
		if (!reiser4_item_statdata(&coord))
			continue;

		/* Opening file by its stat data item denoded by @coord */
		if (!(file = reiser4_file_begin(frag_hint->tree->fs, &coord)))
			continue;

		/* Initializing per-file counters */
		frag_hint->curr = 0;
		frag_hint->fl_bad = 0;
		frag_hint->fl_total = 0;

		if (bogus++ % 16 == 0)
			aal_gauge_update(frag_hint->gauge, 0);

		bogus %= 16;

		/*
		  Calling calculating the file fragmentation by emans of using
		  the function we have seen abowe.
		*/
		if (reiser4_file_layout(file, ffrag_process_blk, data)) {
			aal_exception_error("Can't enumerate data blocks "
					    "occupied by %s", file->name);
			
			reiser4_file_close(file);
			continue;
		}

		/*
		  We was instructed show file fragmentation for each file, not
		  only the average one, we will do it now.
		*/
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

/* Level keeping track for data fragmentation traversal */
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

/* Entry point function for data fragmentation */
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

/* If file is a regular one we show its contant here */
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

/* If file is the directory, we show its contant here */
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

/* Common entry point for --ls and --cat options handling code */
static errno_t debugfs_browse(reiser4_fs_t *fs, char *filename) {
	errno_t res = 0;
	reiser4_file_t *file;
	
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	/* Determining what the type file is */
	if (file->entity->plugin->h.group == REGULAR_FILE)
		res = debugfs_file_cat(file);
	else if (file->entity->plugin->h.group == DIRTORY_FILE)
		res = debugfs_file_ls(file);
	else {
		aal_exception_info("Sorry, browsing of the special files "
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

/* Prints item at passed coord */
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

/* Prints all items belong to the specified file */
static errno_t debugfs_print_file(reiser4_fs_t *fs,
				  char *filename,
				  uint32_t flags)
{
	errno_t res = 0;
	fprint_hint_t hint;
	reiser4_file_t *file;
	
	if (!(file = reiser4_file_open(fs, filename)))
		return -1;

	/*
	  If --show-items option is specified, we show only items belong to the
	  file. If no, that we show all items whihc lie in the same block as the
	  item belong to the file denoted by @filename.
	*/
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
			print_flags |= PF_BLOCK;

			/* Parsing block number */
			if ((blocknr = progs_str2long(optarg, 10)) == INVAL_DIG) {
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

	/* Opening device with file_ops and default blocksize */
	if (!(device = aal_device_open(&file_ops, host_dev,
				       BLOCKSIZE, O_RDONLY)))
	{
		aal_exception_error("Can't open %s. %s.", host_dev,
				    strerror(errno));
		goto error_free_libreiser4;
	}

	/* Open file system on the device */
	if (!(fs = reiser4_fs_open(device, device))) {
		aal_exception_error("Can't open reiser4 on %s", host_dev);
		goto error_free_libreiser4;
	}

	fs->tree->traps.attach = debugfs_attach_handler;
	
	/*
	  Check if few print options specified. If so, and --quiet flay was not
	  applyed we make warning, because that is probably user error and a lot
	  of information will confuse him.
	*/
	if (!aal_pow_of_two(print_flags) && !(behav_flags & BF_QUIET) &&
	    !(print_flags & PF_SITEMS))
	{
		if (aal_exception_yesno("Few print options has been detected. "
					"Continue?") == EXCEPTION_NO)
			goto error_free_fs;
	}

	if (print_flags == 0 && (behav_flags & ~(BF_FORCE | BF_QUIET)) == 0)
		print_flags = PF_SUPER;

	/*
	  Check if specified options are compatible. For instance, --show-each
	  can be used only if --data-frag was specified.
	*/
	if (!(behav_flags & BF_DFRAG) && (behav_flags & BF_SEACH)) {
		aal_exception_warn("Option --show-each is only active if "
				   "--data-frag is specified.");
	}

	/* The same for --print-file option */
	if (!(print_flags & PF_FILE) && (print_flags & PF_SITEMS)) {
		aal_exception_warn("Option --show-items is only active if "
				   "--print-file is specified.");
	}

	/* Handling print options */
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
	
	/* Handling other options */
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

