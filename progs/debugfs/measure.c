/*
  measure.c -- filesystem measurement related code.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h> 
#endif

#include <stdlib.h>
#include "debugfs.h"

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

	aal_assert("umka-1556", frag_hint->level > 0);
	
	*node = NULL;

	/* As we do not need traverse leaf level at all, we going out here */
	if (frag_hint->level <= LEAF_LEVEL)
		return 0;
	
	*node = reiser4_node_open(device, blk);
	return -(*node == NULL);
}

static errno_t tfrag_process_item(
	item_entity_t *item,        /* item we traverse now */
	uint64_t start,             /* region start */
	uint64_t count,             /* region width */
	void *data)                 /* one of blk item points to */
{
	int64_t delta;
	tfrag_hint_t *hint;
	
	hint = (tfrag_hint_t *)data;
	
	if (start == 0)
		return 0;

	delta = hint->curr - start;
				
	if (labs(delta) > 1)
		hint->bad++;
				
	hint->total++;
	hint->curr = start + count - 1;

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
		reiser4_place_t place;

		/* Initializing item at @place */
		if (reiser4_place_open(&place, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		item = &place.item;
		
		/*
		  Checking and calling item's layout method with function
		  tfrag_process_item as a function for handling one block the
		  item points to.
		*/
		if (!item->plugin->item_ops.layout) {
			aal_exception_warn("Item %u in node %llu has not "
					   "\"layout\" method implemented. "
					   "The result will not be releable.",
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
static errno_t tfrag_setup_node(reiser4_place_t *place, void *data) {
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;

	frag_hint->level--;
	return 0;
}

static errno_t tfrag_update_node(reiser4_place_t *place, void *data) {
	tfrag_hint_t *frag_hint = (tfrag_hint_t *)data;

	frag_hint->level++;
	return 0;
}

/*
  Entry point for calculating tree fragmentation. It zeroes out all counters in
  structure which wiil be passed to actual routines and calls tree_traverse
  function with couple of callbacks for handling all traverse cases (open node,
  traverse node, etc). Actual statistics collecting is performed in the passed
  callbacks and subcallbacks (for item traversing).
*/
errno_t debugfs_tree_frag(reiser4_fs_t *fs) {
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
	frag_hint.level = reiser4_node_get_level(root);

	aal_memset(&hint, 0, sizeof(hint));
	
	hint.cleanup = 1;
	hint.data = (void *)&frag_hint;

	aal_gauge_start(gauge);

	/* Calling tree traversal */
	if (reiser4_tree_traverse(fs->tree, &hint, tfrag_open_node,
				  tfrag_process_node, tfrag_setup_node,
				  tfrag_update_node, NULL))
		return -1;

	aal_gauge_free(gauge);

	/* Printing the result */
	printf("%.6f\n", frag_hint.total > 0 ?
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
	uint64_t start,             /* region start */
	uint64_t count,             /* region count */
	void *data)                 /* one of blk item points to */
{
	tstat_hint_t *stat_hint = (tstat_hint_t *)data;
	stat_hint->nodes += count;

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

	level = reiser4_node_get_level(node);

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
			reiser4_place_t place;
			
			if (reiser4_place_open(&place, node, &pos)) {
				aal_exception_error("Can't open item %u in node %llu.", 
						    pos.item, node->blk);
				return -1;
			}

			item = &place.item;
			
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

/* Entry point function for calculating tree statistics */
errno_t debugfs_tree_stat(reiser4_fs_t *fs) {
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
errno_t debugfs_file_frag(reiser4_fs_t *fs,
			  char *filename)
{
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
	printf("%.6f\n", frag_hint.fl_total > 0 ?
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
		reiser4_place_t place;

		/* Initialiing the item at @place */
		if (reiser4_place_open(&place, node, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, node->blk);
			return -1;
		}

		/*
		  If the item is not a stat data item, we getting to the next
		  circle of the loop, because we are intersted only in the stat
		  data items.
		*/
		if (!reiser4_item_statdata(&place))
			continue;

		/* Opening file by its stat data item denoded by @place */
		if (!(file = reiser4_file_begin(frag_hint->tree->fs, &place)))
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
			
			aal_exception_info("Fragmentation for %s: %.6f",
					   file->name, factor);
		}
		
		reiser4_file_close(file);
	}
	
	return 0;
}

/* Level keeping track for data fragmentation traversal */
static errno_t dfrag_setup_node(reiser4_place_t *place, void *data) {
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;
    
	frag_hint->level--;
	return 0;
}

static errno_t dfrag_update_node(reiser4_place_t *place, void *data) {
	ffrag_hint_t *frag_hint = (ffrag_hint_t *)data;

	frag_hint->level++;
	return 0;
}

/* Entry point function for data fragmentation */
errno_t debugfs_data_frag(reiser4_fs_t *fs,
			  uint32_t flags)
{
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
	frag_hint.level = reiser4_tree_height(fs->tree);

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
	
	printf("%.6f\n", frag_hint.fs_total > 0 ?
	       (double)frag_hint.fs_bad / frag_hint.fs_total : 0);
	
	return 0;
}
