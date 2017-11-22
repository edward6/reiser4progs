/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   measurefs.c -- program for measuring reiser4. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

/* Known measurefs behavior flags. */
typedef enum behav_flags {
	BF_FORCE      = 1 << 0,
	BF_YES        = 1 << 1,
	BF_TREE_FRAG  = 1 << 2,
	BF_TREE_STAT  = 1 << 3,
	BF_FILE_FRAG  = 1 << 4,
	BF_DATA_FRAG  = 1 << 5,
	BF_SHOW_FILE  = 1 << 6,
	BF_SHOW_PLUG  = 1 << 7,
	BF_SHOW_PARM  = 1 << 8
} behav_flags_t;

/* Prints measurefs options */
static void measurefs_print_usage(char *name) {
	fprintf(stderr, "Usage: %s [ options ] FILE\n", name);

	fprintf(stderr,
		"Measurement options:\n"
		"  -S, --tree-stat               measures some tree characteristics\n"
		"                                (node packing, etc).\n"
		"  -T, --tree-frag               measures tree fragmentation.\n"
		"  -F, --file-frag FILE          measures fragmentation of specified\n"
		"                                file or directory.\n"
		"  -D, --data-frag               measures average files fragmentation.\n"
		"  -E, --show-file               show file fragmentation for each file\n"
		"                                during calclulation if --data-frag is\n"
		"                                specified.\n"
		"Plugins options:\n"
		"  -p, --print-profile           prints default profile.\n"
		"  -l, --print-plugins           prints known plugins.\n"
	        "  -o, --override TYPE=PLUGIN    overrides the default plugin of the type\n"
	        "                                \"TYPE\" by the plugin \"PLUGIN\" in the\n"
		"                                profile.\n"
		"Common options:\n"
		"  -?, -h, --help                prints program usage.\n"
		"  -V, --version                 prints current version.\n"
		"  -y, --yes                     assumes an answer 'yes' to all questions.\n"
		"  -f, --force                   makes measurefs to use whole disk, not\n"
		"                                block device or mounted partition.\n"
		"  -c, --cache N                 number of nodes in tree buffer cache\n");
}

/* Initializes exception streams used by measurefs */
static void measurefs_init(void) {
	int ex;

	/* Setting up exception streams */
	for (ex = 0; ex < EXCEPTION_TYPE_LAST; ex++)
		misc_exception_set_stream(ex, stderr);
}

typedef struct tree_frag_hint {
	aal_gauge_t *gauge;

	blk_t curr;
	uint16_t level;
	count_t total, bad;
} tree_frag_hint_t;

/* Open node callback for calculating the tree fragmentation */
static reiser4_node_t *tree_frag_open_node(reiser4_tree_t *tree,
				   reiser4_place_t *place, void *data)
{
	reiser4_node_t *node;
	tree_frag_hint_t *frag_hint;

	frag_hint = (tree_frag_hint_t *)data;

	aal_assert("umka-1556", frag_hint->level > 0);
	
	/* As we do not need traverse leaf level at all, we going out here */
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;
	
	node = reiser4_tree_child_node(tree, place);
	return node == NULL ? INVAL_PTR : node;
}

/* Handler for region callback for an item. Its objective is to check if region
   start is not next to current value. If so -- counting bad occurrence. */
static errno_t tree_frag_process_item(uint64_t start, 
				      uint64_t count, 
				      void *data) 
{
	int64_t delta;
	tree_frag_hint_t *hint;
	
	hint = (tree_frag_hint_t *)data;

	/* First time? */
	if (start == 0)
		return 0;

	/* Calculating delta with current value region end. */
	delta = hint->curr - start;

	/* Check if delta is more than one. If so -- bad occurrence. */
	if (labs(delta) > 1)
		hint->bad++;

	/* Counting total regions and updating current blk, which will be used
	   for calculating next delta. */
	hint->total += count;
	hint->curr = start + count - 1;

	return 0;
}

/* Traverse passed @node and calculate tree fragmentation for it. The results
   are stored in @frag_hint structure. This function is called from the tree
   traversal routine for each internal node. See below for details. */
static errno_t tree_frag_process_node(reiser4_node_t *node, void *data) {
	pos_t pos;
	static int bogus = 0;
	tree_frag_hint_t *frag_hint;
	frag_hint = (tree_frag_hint_t *)data;

	pos.unit = MAX_UINT32;

	/* Loop though the node items. */
	for (pos.item = 0; pos.item < reiser4_node_items(node); pos.item++) {
		reiser4_place_t place;

		/* Initializing item at @place */
		if (reiser4_place_open(&place, node, &pos)) {
			aal_error("Can't open item %u in node %llu.",
				  pos.item,
				  (unsigned long long)node->block->nr);
			return -EINVAL;
		}

		/* FIXME: touch every time. show once per second. */
		if (frag_hint->gauge && pos.item == 0 && bogus++ % 128 == 0)
			aal_gauge_touch(frag_hint->gauge);
		
		/* Checking and calling item's layout method with function
		   tfrag_process_item() as a function for handling one block the
		   item points to. */
		if (!place.plug->object->layout)
			continue;

		objcall(&place, object->layout, tree_frag_process_item, data);
	}
	
	frag_hint->level--;
	return 0;
}

static errno_t tree_frag_update_node(reiser4_place_t *place, void *data) {
	((tree_frag_hint_t *)data)->level++;
	return 0;
}

/* Entry point for calculating tree fragmentation. It zeroes out all counters in
   structure which will be passed to actual routines and calls tree_traverse
   function with couple of callbacks for handling all traverse cases (open node,
   traverse node, etc). Actual statistics collecting is performed in the passed
   callbacks and subcallbacks (for item traversing). */
errno_t measurefs_tree_frag(reiser4_fs_t *fs, uint32_t flags) {
	tree_frag_hint_t frag_hint;
	errno_t res;

	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	if (!(flags & BF_YES)) {
		/* Initializing gauge, because it is a long process and user
		   should be informed what the stage of the process is going at
		   the moment. */
		frag_hint.gauge = 
			aal_gauge_create(aux_gauge_handlers[GT_PROGRESS],
					 NULL, NULL, 0, "Tree fragmentation "
					 "... ");
		if (!frag_hint.gauge)
			return -ENOMEM;
	}
	
	/* Preparing serve structure, statistics will be stored in  */
	frag_hint.curr = reiser4_tree_get_root(fs->tree);
	frag_hint.level = reiser4_tree_get_height(fs->tree);
	
	if (frag_hint.gauge)
		aal_gauge_touch(frag_hint.gauge);

	/* Calling tree traversal with callbacks for processing internal nodes
	   and items in order to calculate tree fragmentation. */
	if ((res = reiser4_tree_trav(fs->tree, tree_frag_open_node,
				     tree_frag_process_node,
				     tree_frag_update_node,
				     NULL, &frag_hint)))
	{
		return res;
	}

	/* Printing results. */
	if (frag_hint.gauge) {
		aal_gauge_done(frag_hint.gauge);
		aal_gauge_free(frag_hint.gauge);
	} else {
		printf("Tree fragmentation: ");
	}

	printf("%.6f\n", frag_hint.total > 0 ?
	       (double)frag_hint.bad / frag_hint.total : 0);
	
	return 0;
};

typedef struct tree_stat_hint {
	aal_gauge_t *gauge;
	reiser4_tree_t *tree;

	uint64_t nodes;
	uint64_t twigs;
	uint64_t leaves;
	uint64_t branches;
	uint64_t formatted;

	uint64_t items;
	uint64_t tails;
	uint64_t extents;
	uint64_t nodeptrs;
	uint64_t statdatas;
	uint64_t direntries;

	double twigs_used;
	double leaves_used;
	double branches_used;
	double formatted_used;

	reiser4_place_t *place;
} tree_stat_hint_t;

/* Process one item on level > LEAF_LEVEL. */
static errno_t stat_item_layout(uint64_t start, uint64_t width, void *data) {
	tree_stat_hint_t *stat_hint;
	reiser4_place_t *place;
	uint32_t blksize;

	stat_hint = (tree_stat_hint_t *)data;
	place = stat_hint->place;

	if (place->plug->p.id.group == EXTENT_ITEM) {
		reiser4_master_t *master;
			
		master = stat_hint->tree->fs->master;
		blksize = reiser4_master_get_blksize(master);

		stat_hint->leaves_used = blksize +
			(stat_hint->leaves_used * stat_hint->leaves);
		
		stat_hint->leaves_used /= (stat_hint->leaves + 1);

		stat_hint->nodes += width;
		stat_hint->leaves += width;
	}
	
	return 0;
}

/* Processing one formatted node and calculate number of internal pointers,
   extent ones, packing, etc. */
static errno_t stat_process_node(reiser4_node_t *node, void *data) {
	uint8_t level;
	uint32_t blksize;
	uint32_t twigs_used;
	uint32_t leaves_used;
	uint32_t branches_used;
	uint32_t formatted_used;
	
	reiser4_tree_t *tree = (reiser4_tree_t *)node->tree;

	tree_stat_hint_t *stat_hint;
	pos_t pos = {MAX_UINT32, MAX_UINT32};

	stat_hint = (tree_stat_hint_t *)data;
	level = reiser4_node_get_level(node);

	blksize = reiser4_master_get_blksize(tree->fs->master);

	/* FIXME: touch eaery time. show once per second. */
	if (stat_hint->gauge && stat_hint->formatted % 128 == 0)
		aal_gauge_touch(stat_hint->gauge);

	formatted_used = blksize - reiser4_node_space(node);

	stat_hint->formatted_used = formatted_used +
		(stat_hint->formatted_used * stat_hint->formatted);

	stat_hint->formatted_used /= (stat_hint->formatted + 1);

	if (level == LEAF_LEVEL) {
		/* Calculating leaves packing. */
		leaves_used = blksize - reiser4_node_space(node);

		stat_hint->leaves_used = leaves_used +
			(stat_hint->leaves_used * stat_hint->leaves);
		
		stat_hint->leaves_used /= (stat_hint->leaves + 1);
	} else if (level == TWIG_LEVEL) {
		/* Calculating twig nodes packing. */
		twigs_used = blksize - reiser4_node_space(node);
		
		stat_hint->twigs_used = twigs_used +
			(stat_hint->twigs_used * stat_hint->twigs);

		stat_hint->twigs_used /= (stat_hint->twigs + 1);
	} else {
		/* Calculating branch nodes packing. */
		branches_used = blksize - reiser4_node_space(node);
		
		stat_hint->branches_used = branches_used +
			(stat_hint->branches_used * stat_hint->branches);

		stat_hint->branches_used /= (stat_hint->branches + 1);
	}

	/* Loop through all node items and calling item->layout() method in order
	   to calculate all blocks item references.*/
	for (pos.item = 0; pos.item < reiser4_node_items(node);
	     pos.item++)
	{
		errno_t res;
		reiser4_place_t place;

		/* Fetching item data. */
		if ((res = reiser4_place_open(&place, node, &pos))) {
			aal_error("Can't open item %u in node %llu.",
				  pos.item,
				  (unsigned long long)node->block->nr);
			return res;
		}

		/* Calculating item count. This probably should be done in more
		   item type independent manner. */
		stat_hint->items++;

		switch (place.plug->p.id.group) {
		case STAT_ITEM:
			stat_hint->statdatas++;
			break;
		case EXTENT_ITEM:
			stat_hint->extents++;
			break;
		case PTR_ITEM:
			stat_hint->nodeptrs++;
			break;
		case TAIL_ITEM:
			stat_hint->tails++;
			break;
		case DIR_ITEM:
			stat_hint->direntries++;
			break;
		}

		/* Calling layout() method with callback for counting referenced
		   blocks. */
		if (!place.plug->object->layout)
			continue;

		stat_hint->place = &place;
		objcall(&place, object->layout, stat_item_layout, data);
	}

	/* Updating common counters like nodes traversed at all, formatted ones,
	   etc. This will be used later. */
	stat_hint->nodes++;
	stat_hint->formatted++;
	
	stat_hint->twigs += (level == TWIG_LEVEL);
	stat_hint->leaves += (level == LEAF_LEVEL);
	stat_hint->branches += (level > TWIG_LEVEL);

	return 0;
}

/* Entry point function for calculating tree statistics */
errno_t measurefs_tree_stat(reiser4_fs_t *fs, uint32_t flags) {
	tree_stat_hint_t stat_hint;
	uint32_t blksize;
	errno_t res;

	aal_memset(&stat_hint, 0, sizeof(stat_hint));

	/* Creating gauge. */
	if (!(flags & BF_YES)) {
		stat_hint.gauge = 
			aal_gauge_create(aux_gauge_handlers[GT_PROGRESS],
					 NULL, NULL, 0, "Tree statistics ... ");
		if (!stat_hint.gauge)
			return -ENOMEM;
	}

	/* Traversing tree with callbacks for calculating tree statistics. */
	if (stat_hint.gauge)
		aal_gauge_touch(stat_hint.gauge);

	stat_hint.tree = fs->tree;
	
	if ((res = reiser4_tree_trav(fs->tree, NULL, stat_process_node,
				     NULL, NULL, &stat_hint)))
	{
		return res;
	}

	if (stat_hint.gauge) {
		aal_gauge_done(stat_hint.gauge);
		aal_gauge_free(stat_hint.gauge);
		misc_wipe_line(stdout);
	}

	blksize = reiser4_master_get_blksize(fs->master);
	
	/* Printing results. */
	printf("Packing statistics:\n");
	
	printf("  Formatted nodes:%*.2fb (%.2f%%)\n",
	       11, stat_hint.formatted_used,
	       (stat_hint.formatted_used * 100) / blksize);
	
	printf("  Branch nodes:%*.2fb (%.2f%%)\n",
	       14, stat_hint.branches_used,
	       (stat_hint.branches_used * 100) / blksize);
	
	printf("  Twig nodes:%*.2fb (%.2f%%)\n",
	       16, stat_hint.twigs_used,
	       (stat_hint.twigs_used * 100) / blksize);
	
	printf("  Leaf nodes:%*.2fb (%.2f%%)\n\n",
	       16, stat_hint.leaves_used,
	       (stat_hint.leaves_used * 100) / blksize);

	printf("Node statistics:\n");
	printf("  Total nodes:%*llu\n", 15,
	       (unsigned long long)stat_hint.nodes);

	printf("  Formatted nodes:%*llu\n", 11,
	       (unsigned long long)stat_hint.formatted);

	printf("  Unformatted nodes:%*llu\n", 9,
	       (unsigned long long)(stat_hint.nodes - stat_hint.formatted));

	printf("  Branch nodes:%*llu\n", 14,
	       (unsigned long long)stat_hint.branches);
	printf("  Twig nodes:%*llu\n", 16,
	       (unsigned long long)stat_hint.twigs);
	printf("  Leaf nodes:%*llu\n\n", 16,
	       (unsigned long long)stat_hint.leaves);
	
	printf("Item statistics:\n");
	printf("  Total items:%*llu\n", 15,
	       (unsigned long long)stat_hint.items);
	printf("  Nodeptr items:%*llu\n", 13,
	       (unsigned long long)stat_hint.nodeptrs);
	printf("  Statdata items:%*llu\n", 11,
	       (unsigned long long)stat_hint.statdatas);
	printf("  Direntry items:%*llu\n", 12,
	       (unsigned long long)stat_hint.direntries);
	printf("  Tail items:%*llu\n", 16,
	       (unsigned long long)stat_hint.tails);
	printf("  Extent items:%*llu\n", 14,
	       (unsigned long long)stat_hint.extents);
	return 0;
}

typedef struct file_frag_hint {
	aal_gauge_t *gauge;

	count_t bad;
	count_t total;

	blk_t last;
	count_t files;
	
	double current;
	uint32_t flags;
	uint16_t level;
} file_frag_hint_t;

/* Callback function for processing one block belong to the file we are
   traversing. */
static errno_t file_frag_process_blk(blk_t start, count_t width, void *data) {
	int64_t delta;
	file_frag_hint_t *frag_hint;

	frag_hint = (file_frag_hint_t *)data;

	/* Check if we are went here first time */
	if (frag_hint->last > 0) {
		delta = frag_hint->last - start;

		if (labs(delta) > 1)
			frag_hint->bad++;
	}

	frag_hint->total += width;
	frag_hint->last = start + width - 1;
	
	return 0;
}

/* Calculates the passed file fragmentation. */
errno_t measurefs_file_frag(reiser4_fs_t *fs, char *filename, uint32_t gauge) {
	errno_t res = 0;
	reiser4_object_t *object;
	file_frag_hint_t frag_hint;

	/* Opens object by its name */
	if (!(object = reiser4_semantic_open(fs->tree, filename, NULL, 0)))
		return -EINVAL;

	/* Initializing serve structures */
	aal_memset(&frag_hint, 0, sizeof(frag_hint));
	
	/* Calling file layout function, which calls file_frag_process_blk()
	   fucntion for each block belong to the file @filename. */
	if ((res = reiser4_object_layout(object, file_frag_process_blk,
					 &frag_hint)))
	{
		aal_error("Can't enumerate data blocks "
			  "occupied by %s", filename);
		goto error_free_object;
	}
	
	reiser4_object_close(object);

	/* Printing results */
	printf("Fragmentation for %s is %.6f\n", filename,
	       frag_hint.total > 0 ? (double)frag_hint.bad /
	       frag_hint.total : 0);
	
	return 0;

 error_free_object:
	reiser4_object_close(object);
	return res;
}

/* Processes leaf node in order to find all stat data items which are start of
   corresponding files and calculate file fragmentation for each of them. */
static errno_t data_frag_process_node(reiser4_node_t *node, void *data) {
	reiser4_tree_t *tree = (reiser4_tree_t *)node->tree;
	file_frag_hint_t *frag_hint;
	pos_t pos;

	pos.unit = MAX_UINT32;
	frag_hint = (file_frag_hint_t *)data;

	frag_hint->level--;
	
	if (frag_hint->level > LEAF_LEVEL)
		return 0;
	
	/* The loop through all the items in current node */
	for (pos.item = 0; pos.item < reiser4_node_items(node);
	     pos.item++)
	{
		errno_t res;
		reiser4_place_t place;
		reiser4_object_t *object;

		/* Initialiing the item at @place */
		if ((res = reiser4_place_open(&place, node, &pos))) {
			aal_error("Can't open item %u in node %llu.",
				  pos.item,
				  (unsigned long long)node->block->nr);
			return res;
		}

		if (!reiser4_item_statdata(&place))
			continue;

		/* Opening object by its stat data item denoted by @place */
		if (!(object = reiser4_object_open(tree, NULL, &place)))
			continue;

		/* Initializing per-file counters */
		frag_hint->bad = 0;
		frag_hint->last = 0;
		frag_hint->total = 0;

		if (frag_hint->gauge && pos.item == 0)
			aal_gauge_touch(frag_hint->gauge);

		/* Calling calculating the file fragmentation by means of using
		   the function we have seen above. */
		if (reiser4_object_layout(object, file_frag_process_blk, data)) {
			aal_error("Can't enumerate data blocks occupied by %s", 
				  reiser4_print_inode(&object->info.object));
			goto error_close_object;
		}

		if (frag_hint->total > 0) {
			frag_hint->current += (double)frag_hint->bad /
				frag_hint->total;
		}

		frag_hint->files++;
			
		/* We was instructed show file fragmentation for each file, not
		   only the average one, we will do it now. */
		if (frag_hint->flags & BF_SHOW_FILE) {
			double file_factor = frag_hint->total > 0 ?
				(double)frag_hint->bad / frag_hint->total : 0;
			
			double curr_factor = frag_hint->files > 0 ?
				(double)frag_hint->current / frag_hint->files : 0;
			
			aal_mess("Fragmentation for %s: %.6f [av. %.6f ]",
				 reiser4_print_inode(&object->info.object),
				 file_factor, curr_factor);
		}

	error_close_object:
		reiser4_object_close(object);
	}
	
	return 0;
}

static errno_t data_frag_update_node(reiser4_place_t *place, void *data) {
	((file_frag_hint_t *)data)->level++;
	return 0;
}

/* Entry point function for data fragmentation. */
errno_t measurefs_data_frag(reiser4_fs_t *fs, uint32_t flags) {
	file_frag_hint_t frag_hint;
	errno_t res;

	aal_memset(&frag_hint, 0, sizeof(frag_hint));

	/* Create gauge. */
	if (!(flags & BF_YES)) {
		frag_hint.gauge = 
			aal_gauge_create(aux_gauge_handlers[GT_PROGRESS],
					 NULL, NULL, 0, "Data fragmentation "
					 "... ");

		if (!frag_hint.gauge) {
			aal_fatal("Out of memory!");
			return -ENOMEM;
		}
	}

	frag_hint.flags = flags;
	frag_hint.level = reiser4_tree_get_height(fs->tree);
	
	if (frag_hint.gauge)
		aal_gauge_touch(frag_hint.gauge);
	
	if ((res = reiser4_tree_trav(fs->tree, NULL, data_frag_process_node,
				     data_frag_update_node, NULL, &frag_hint)))
		return res;

	if (frag_hint.gauge) {
		aal_gauge_done(frag_hint.gauge);
		aal_gauge_free(frag_hint.gauge);
	}

	if (frag_hint.flags & BF_SHOW_FILE || !frag_hint.gauge)
		printf("Data fragmentation is: ");
	
	printf("%.6f\n", frag_hint.files > 0 ?
	       (double)frag_hint.current / frag_hint.files : 0);
	
	return 0;
}

int main(int argc, char *argv[]) {
	int c;
	char *host_dev;

	uint32_t cache;
	uint32_t flags = 0;
	char override[4096];

	reiser4_fs_t *fs;
	aal_device_t *device;
	char *frag_filename = NULL;
	
	static struct option long_options[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"force", no_argument, NULL, 'f'},
		{"yes", no_argument, NULL, 'y'},
		{"tree-stat", no_argument, NULL, 'S'},
		{"tree-frag", no_argument, NULL, 'T'},
		{"file-frag", required_argument, NULL, 'F'},
		{"data-frag", no_argument, NULL, 'D'},
		{"show-file", no_argument, NULL, 'E'},
		{"print-profile", no_argument, NULL, 'p'},
		{"print-plugins", no_argument, NULL, 'l'},
		{"override", required_argument, NULL, 'o'},
		{"cache", required_argument, NULL, 'c'},
		{0, 0, 0, 0}
	};

	measurefs_init();
	memset(override, 0, sizeof(override));

	if (argc < 2) {
		measurefs_print_usage(argv[0]);
		return USER_ERROR;
	}

	/* Parsing parameters */
	while ((c = getopt_long(argc, argv, "hVyfKTDESF:o:plc:?",
				long_options, (int *)0)) != EOF)
	{
		switch (c) {
		case 'h':
		case '?':
			measurefs_print_usage(argv[0]);
			return NO_ERROR;
		case 'V':
			misc_print_banner(argv[0]);
			return NO_ERROR;
		case 'S':
			flags |= BF_TREE_STAT;
			break;
		case 'T':
			flags |= BF_TREE_FRAG;
			break;
		case 'D':
			flags |= BF_DATA_FRAG;
			break;
		case 'F':
			flags |= BF_FILE_FRAG;
			frag_filename = optarg;
			break;
		case 'E':
			flags |= BF_SHOW_FILE;
			break;
		case 'f':
			flags |= BF_FORCE;
			break;
		case 'y':
			flags |= BF_YES;
			break;
		case 'p':
			flags |= BF_SHOW_PARM;
			break;
		case 'l':
			flags |= BF_SHOW_PLUG;
			break;
		case 'o':
			aal_strncat(override, optarg,
				    aal_strlen(optarg));
			
			aal_strncat(override, ",", 1);
			break;
		case 'c':
			if ((cache = misc_str2long(optarg, 10)) == INVAL_DIG) {
				aal_error("Invalid cache value specified (%s).",
					  optarg);
				return USER_ERROR;
			}

			misc_mpressure_setup(cache);
			break;
		}
	}

	if (!(flags & BF_YES))
		misc_print_banner(argv[0]);

	if (libreiser4_init()) {
		aal_error("Can't initialize libreiser4.");
		goto error;
	}

	/* Overriding default params by passed values. This should be done after
	   libreiser4 is initialized. */
	if (aal_strlen(override) > 0) {
		override[aal_strlen(override) - 1] = '\0';
		
		if (!(flags & BF_YES)) {
			aal_mess("Overriding the plugin profile by \"%s\".", 
				 override);
		}
		
		if (misc_profile_override(override))
			goto error_free_libreiser4;
	}
	
	if (flags & BF_SHOW_PARM)
		misc_profile_print();

	if (flags & BF_SHOW_PLUG)
		misc_plugins_print();

	if (optind >= argc) {
		if (!(flags & BF_SHOW_PARM) && !(flags & BF_SHOW_PLUG))
			measurefs_print_usage(argv[0]);
			
		goto error_free_libreiser4;
	}
	
	host_dev = argv[optind];

	/* Checking if passed partition is mounted */
	if (misc_dev_mounted(host_dev) > 0 && !(flags & BF_FORCE)) {
		aal_error("Device %s is mounted at the moment. "
			  "Use -f to force over.", host_dev);
		goto error_free_libreiser4;
	}

	/* Opening device with file_ops and default blocksize */
	if (!(device = aal_device_open(&file_ops, host_dev,
				       512, O_RDONLY)))
	{
		aal_error("Can't open %s. %s.", host_dev,
			  strerror(errno));
		goto error_free_libreiser4;
	}

	/* Open file system on the device */
	if (!(fs = reiser4_fs_open(device, 1))) {
		aal_error("Can't open reiser4 on %s",
			  host_dev);
		goto error_free_device;
	}

	fs->tree->mpc_func = misc_mpressure_detect;

	/* Check if specified options are compatible. For instance, --show-each
	   can be used only if --data-frag was specified. */
	if (!(flags & BF_DATA_FRAG) && (flags & BF_SHOW_FILE)) {
		aal_warn("Option --show-file is only active if "
			 "--data-frag is specified.");
	}

	if (!(flags & BF_TREE_FRAG || flags & BF_DATA_FRAG ||
	      flags & BF_FILE_FRAG || flags & BF_TREE_STAT))
	{
		flags |= BF_TREE_STAT;
	}

	/* Handling measurements options */
	if (flags & BF_TREE_FRAG) {
		if (measurefs_tree_frag(fs, flags))
			goto error_free_fs;
	}

	if (flags & BF_DATA_FRAG) {
		if (measurefs_data_frag(fs, flags))
			goto error_free_fs;
	}

	if (flags & BF_FILE_FRAG) {
		if (measurefs_file_frag(fs, frag_filename,
					flags))
			goto error_free_fs;
	}
	
	if (flags & BF_TREE_STAT) {
		if (measurefs_tree_stat(fs, flags))
			goto error_free_fs;
	}

	/* Deinitializing filesystem instance and device instance */
	reiser4_fs_close(fs);
	aal_device_close(device);

	/* Deinitializing libreiser4. At the moment only plugins are unloading
	   during this. */
	libreiser4_fini();
	return NO_ERROR;

 error_free_fs:
	reiser4_fs_close(fs);
 error_free_device:
	aal_device_close(device);
 error_free_libreiser4:
	libreiser4_fini();
 error:
	return OPER_ERROR;
}
