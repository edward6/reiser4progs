/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    repair/add_missing.c -- the common methods for insertion leaves and extent
    item from twigs unconnected from the tree. */

#include <repair/add_missing.h>

/* Callback for item_ops->layout method to mark all the blocks, items points 
   to, in the allocator. */
static errno_t callback_item_mark_region(void *object, uint64_t start, 
					 uint64_t count, void *data)
{
	repair_am_t *am = (repair_am_t *)data;
	
	aal_assert("vpf-735", data != NULL);
	
	if (start != 0) {
		/* These blocks are marked in allocator as it already has all 
		   blocks forbidden for allocation marked. Just mark them as 
		   used now. */
		aux_bitmap_mark_region(am->bm_used, start, count);
	}
	
	return 0;
}

/* Callback for traverse through all items of the node. Calls for the item, 
   determined by place, layout method, if it is not the branch and has pointers
   to some blocks. */
static errno_t callback_layout(reiser4_place_t *place, void *data) {
	aal_assert("vpf-649", place != NULL);
	aal_assert("vpf-748", !reiser4_item_branch(place->plug));

	if (!place->plug->o.item_ops->object->layout)
		return 0;
	
	/* All these blocks should not be used in the allocator and should be 
	   forbidden for allocation. Check it somehow first. */
	return plug_call(place->plug->o.item_ops->object, layout,
			 (place_t *)place, callback_item_mark_region, data);
}

static void repair_add_missing_setup(repair_am_t *am) {
	aal_assert("vpf-887", am != NULL);
	
	aal_memset(am->progress, 0, sizeof(*am->progress));
	
	if (!am->progress_handler)
		return;
	
	am->progress->type = GAUGE_PERCENTAGE;
	
	am->progress->text = "***** AddMissing Pass: inserting unconnected "
		"nodes into the tree.";
	
	am->progress->state = PROGRESS_START;
	time(&am->stat.time);
	am->progress_handler(am->progress);
	am->progress->text = NULL;
}

static void repair_add_missing_update(repair_am_t *am) {
	repair_am_stat_t *stat;
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-886", am != NULL);
	
	if (!am->progress_handler)
		return;
	
	am->progress->state = PROGRESS_END;
	am->progress_handler(am->progress);    
	
	stat = &am->stat;
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	aal_stream_format(&stream, "\tTwigs: read %llu, inserted %llu, by "
			  "items %llu\n", stat->read_twigs, stat->by_twig,
			  stat->by_item_twigs);
	    
	aal_stream_format(&stream, "\tLeaves: read %llu, inserted %llu, by "
			  "items %llu\n", stat->read_leaves, stat->by_leaf, 
			  stat->by_item_leaves);

	time_str = ctime(&am->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&am->stat.time);
	time_str = ctime(&am->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);
	
	am->progress->state = PROGRESS_STAT;
	am->progress->text = (char *)stream.entity;
	am->progress_handler(am->progress);
	
	aal_stream_fini(&stream);
}

static errno_t repair_am_node_prepare(repair_am_t *am, reiser4_node_t *node) {
	reiser4_place_t place;
	trans_hint_t hint;
	uint32_t count;
	errno_t res;
	
	/* Remove all metadata items from the node before insertion. */
	place.node = node;
	place.pos.unit = MAX_UINT32;
	count = reiser4_node_items(node);

	for (place.pos.item = 0; place.pos.item < count; place.pos.item++) {
		if ((res = reiser4_place_fetch(&place))) {
			aal_exception_error("Node (%llu), item (%u): failed to "
					    "open the item.",node_blocknr(node),
					    place.pos.item);

			return res;
		}

		/* If this is an index item of the tree, remove it. */
		if (!reiser4_item_branch(place.plug)) 
			continue;

		hint.count = 1;

		if ((res = reiser4_node_remove(place.node, &place.pos, &hint)))
			return res;

		place.pos.item--;
		count--;
	}
	
	return 0;
}

static errno_t repair_am_blk_free(repair_am_t *am, blk_t blk) {
	aal_assert("vpf-1330", am != NULL);

	return reiser4_alloc_release(am->repair->fs->alloc, blk, 1);
}

static errno_t repair_am_blk_used(repair_am_t *am, blk_t blk) {
	aal_assert("vpf-1330", am != NULL);

	/* These blocks are marked in allocator as it already has all blocks 
	   forbidden for allocation marked. Just mark them as used now. */
	aux_bitmap_mark_region(am->bm_used, blk, 1);

	return 0;
}

typedef struct stat_bitmap {
	uint64_t read, by_node, by_item;
} stat_bitmap_t;

static errno_t repair_am_nodes_insert(repair_am_t *am, aux_bitmap_t *bitmap,
				      stat_bitmap_t *stat)
{
	reiser4_alloc_t *alloc;
	reiser4_node_t *node;
	errno_t res;
	blk_t blk;
	
	aal_assert("vpf-1282", am != NULL);
	aal_assert("vpf-1283", bitmap != NULL);
	aal_assert("vpf-1284", stat != NULL);
	
	alloc = am->repair->fs->alloc;
	
	blk = 0;
	/* Try to insert the whole twig/leaf at once. If it can be 
	   inserted only after splitting the node found by lookup 
	   into 2 nodes -- it will be done instead of following item
	   by item insertion. */
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
		aal_assert("vpf-896", !reiser4_alloc_occupied(alloc, blk, 1));

		node = reiser4_node_open(am->repair->fs->tree, blk);
		stat->read++;

		if (am->progress_handler)
			am->progress_handler(am->progress);

		if (node == NULL) {
			aal_exception_fatal("Add Missing pass failed to "
					    "open the node (%llu)", blk);

			return -EINVAL;
		}

		/* Prepare the node for the insertion. */
		if ((res = repair_am_node_prepare(am, node))) 
			goto error_close_node;

		if (reiser4_node_items(node) == 0) {
			aux_bitmap_clear(bitmap, node_blocknr(node));
			repair_am_blk_free(am, node_blocknr(node));
			reiser4_node_close(node);
			blk++;
			continue;
		}

		res = repair_tree_attach(am->repair->fs->tree, node);
		if (res < 0 && res != -ESTRUCT) {
			aal_exception_bug("Add missing pass failed to attach "
					  "the node (%llu) to the tree.", blk);

			goto error_close_node;
		} else if (res == 0) {
			/* Has been inserted. */
			aux_bitmap_clear(bitmap, node_blocknr(node));
			repair_am_blk_used(am, node_blocknr(node));

			stat->by_node++;

			res = repair_node_traverse(node, callback_layout, am);
			if (res) goto error_close_node;

			blk++;
		} /* if res > 0 - uninsertable case - insert by items later. */
	}

	return 0;

 error_close_node:
	reiser4_node_close(node);
	return res;
}

static errno_t repair_am_items_insert(repair_am_t *am, aux_bitmap_t *bitmap, 
				      stat_bitmap_t *stat)
{
	reiser4_node_t *node;
	uint32_t count;
	errno_t res;
	blk_t blk;

	aal_assert("vpf-1285", am != NULL);
	aal_assert("vpf-1286", bitmap != NULL);
	aal_assert("vpf-1287", stat != NULL);
	
	blk = 0;
	/* Insert extents from the twigs/all items from leaves which 
	   are not in the tree yet item-by-item into the tree, overwrite
	   existent data which is in the tree already if needed. 
	   FIXME: overwriting should be done on the base of flush_id. */
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
#ifdef ENABLE_DEBUG
		reiser4_alloc_t *alloc = am->repair->fs->alloc;
#endif
			
		reiser4_place_t place;
		pos_t *pos = &place.pos;

		aal_assert("vpf-897", !reiser4_alloc_occupied(alloc, blk, 1));

		node = reiser4_node_open(am->repair->fs->tree, blk);

		if (am->progress_handler)
			am->progress_handler(am->progress);

		if (node == NULL) {
			aal_exception_fatal("Add Missing pass failed to "
					    "open the node (%llu)", blk);
			return -EINVAL;
		}

		pos->unit = MAX_UINT32;
		count = reiser4_node_items(node);
		place.node = node;

		stat->by_item++;

		for (pos->item = 0; pos->item < count; pos->item++) {
			aal_assert("vpf-636", pos->unit == MAX_UINT32);

			if ((res = reiser4_place_fetch(&place))) {
				aal_exception_error("Node (%llu), item (%u): "
						    "cannot open the item "
						    "place.", blk, pos->item);

				goto error_close_node;
			}

			if ((res = repair_tree_insert(am->repair->fs->tree, 
						      &place)) < 0)
				goto error_close_node;

			if (res == 0) {
				/* FIXME-VITALY: this is wrong. Fix it when merge 
				   will be ready. */
				if ((res = callback_layout(&place, am)))
					goto error_close_node;
			}
		}

		aux_bitmap_clear(bitmap, node_blocknr(node));
		repair_am_blk_free(am, node_blocknr(node));
		reiser4_node_close(node);
		blk++;
	}

	return 0;

 error_close_node:
	reiser4_node_close(node);
	return res;
}

/* The pass inself, adds all the data which are not in the tree yet and which 
   were found on the partition during the previous passes. */
errno_t repair_add_missing(repair_am_t *am) {
	repair_progress_t progress;
	aux_bitmap_t *bitmap;
	stat_bitmap_t stat;
	uint32_t bnum;
	errno_t res;
	
	aal_assert("vpf-595", am != NULL);
	aal_assert("vpf-846", am->repair != NULL);
	aal_assert("vpf-847", am->repair->fs != NULL);
	aal_assert("vpf-848", am->bm_twig != NULL);
	aal_assert("vpf-849", am->bm_leaf != NULL);
	
	am->progress = &progress;
	
	repair_add_missing_setup(am);
	
	
	/* 2 loops - 1 for twigs, another for leaves. */
	for (bnum = 0; bnum < 2; bnum++) {
		if (bnum) {
			bitmap = am->bm_leaf;
			am->progress->text = "Inserting unconnected leaves: ";
			am->progress->u.rate.total = 
				aux_bitmap_marked(am->bm_leaf);
		} else {
			bitmap = am->bm_twig;
			am->progress->text = "Inserting unconnected twigs: ";
			am->progress->u.rate.total = 
				aux_bitmap_marked(am->bm_twig);
		}
		
		/* Debugging of item coping. */
		if (am->repair->debug_flag) 
			goto debug;
		
		am->progress->u.rate.done = 0;
		am->progress->state = PROGRESS_UPDATE;
		
		aal_memset(&stat, 0, sizeof(stat));
		
		if ((res = repair_am_nodes_insert(am, bitmap, &stat)))
			goto error;
		
		if (bnum) {
			am->stat.read_twigs = stat.read;
			am->stat.by_twig = stat.by_node;
		} else {
			am->stat.read_leaves = stat.read;
			am->stat.by_leaf = stat.by_node;
		}
	debug:
		am->progress->u.rate.done = 0;
		if (bnum) {
			am->progress->text = "Inserting unconnected twigs "
				"item-by-item: ";
			am->progress->u.rate.total = 
				aux_bitmap_marked(am->bm_leaf);
		} else {
			am->progress->text = "Inserting unconnected leaves "
				"item-by-item: ";
			am->progress->u.rate.total = 
				aux_bitmap_marked(am->bm_twig);
		} 
		
		aal_memset(&stat, 0, sizeof(stat));
		
		if ((res = repair_am_items_insert(am, bitmap, &stat)))
			goto error;

		if (bnum)
			am->stat.by_item_leaves = stat.by_item;
		else
			am->stat.by_item_twigs = stat.by_item;
	}
	
	repair_add_missing_update(am);
	reiser4_tree_sync(am->repair->fs->tree);
	return 0;

 error:
	am->progress->state = PROGRESS_END;
	if (am->progress_handler)
		am->progress_handler(am->progress);
	
	repair_add_missing_update(am);
	reiser4_fs_sync(am->repair->fs);
	return res;
}

