/*  Copyright 2001-2005 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    repair/add_missing.c -- the common methods for insertion leaves and extent
    item from twigs unconnected from the tree. */

#include <repair/add_missing.h>

/* Callback for item_ops->layout method to mark all the blocks, items points 
   to, in the allocator. */
static errno_t cb_item_mark_region(uint64_t start, uint64_t count, void *data) {
	repair_am_t *am = (repair_am_t *)data;
	
	aal_assert("vpf-735", data != NULL);
	
	if (start != 0) {
		/* These blocks are marked in allocator as it already has all 
		   blocks forbidden for allocation marked. Just mark them as 
		   used now. */
		reiser4_bitmap_mark_region(am->bm_used, start, count);
	}
	
	return 0;
}

/* Callback for traverse through all items of the node. Calls for the item, 
   determined by place, layout method, if it is not the branch and has pointers
   to some blocks. */
static errno_t cb_layout(reiser4_place_t *place, void *data) {
	aal_assert("vpf-649", place != NULL);
	aal_assert("vpf-748", !reiser4_item_branch(place->plug));

	if (!place->plug->object->layout)
		return 0;
	
	/* All these blocks should not be used in the allocator and should be 
	   forbidden for allocation. Check it somehow first. */
	return objcall(place, object->layout, cb_item_mark_region, data);
}

static void repair_add_missing_update(repair_am_t *am) {
	repair_am_stat_t *stat;
	aal_stream_t stream;
	char *time_str;
	
	aal_assert("vpf-886", am != NULL);
	
	stat = &am->stat;
	
	aal_stream_init(&stream, NULL, &memory_stream);
	
	aal_stream_format(&stream, "\tTwigs: read %llu, inserted %llu, "
			  "by item %llu, empty %llu\n", stat->read_twigs, 
			  stat->by_twig, stat->by_item_twigs,
			  stat->empty);
	
	aal_stream_format(&stream, "\tLeaves: read %llu, inserted %llu, by "
			  "item %llu\n", stat->read_leaves, stat->by_leaf, 
			  stat->by_item_leaves);

	time_str = ctime(&am->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
	time(&am->stat.time);
	time_str = ctime(&am->stat.time);
	time_str[aal_strlen(time_str) - 1] = '\0';
	aal_stream_format(&stream, time_str);
	aal_mess(stream.entity);
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
			aal_error("Node (%llu), item (%u): failed to open the "
				  "item.",
				  (unsigned long long)node->block->nr,
				  place.pos.item);
			return res;
		}

		/* If this is an index item of the tree, remove it. */
		if (!reiser4_item_branch(place.plug)) 
			continue;

		hint.count = 1;
		hint.place_func = NULL;
		hint.region_func = NULL;
		hint.shift_flags = SF_DEFAULT;

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
	reiser4_bitmap_mark_region(am->bm_used, blk, 1);

	return 0;
}

typedef struct stat_bitmap {
	uint64_t read, by_node, by_item, empty;
} stat_bitmap_t;

static errno_t repair_am_nodes_insert(repair_am_t *am, 
				      reiser4_bitmap_t *bitmap,
				      stat_bitmap_t *stat)
{
	reiser4_node_t *node;
	uint64_t total;
	errno_t res;
	blk_t blk;
	
	aal_assert("vpf-1282", am != NULL);
	aal_assert("vpf-1283", bitmap != NULL);
	aal_assert("vpf-1284", stat != NULL);
	
	total = reiser4_bitmap_marked(bitmap);
	
	blk = 0;
	/* Try to insert the whole twig/leaf at once. If it can be 
	   inserted only after splitting the node found by lookup 
	   into 2 nodes -- it will be done instead of following item
	   by item insertion. */
	while ((blk = reiser4_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
		node = reiser4_node_open(am->repair->fs->tree, blk);
		stat->read++;
		aal_gauge_set_value(am->gauge, stat->read * 100 / total);
		aal_gauge_touch(am->gauge);

		if (node == NULL) {
			aal_error("Add Missing pass failed to "
				  "open the node (%llu)",
				  (unsigned long long)blk);

			return -EINVAL;
		}

		/* Prepare the node for the insertion. */
		if ((res = repair_am_node_prepare(am, node))) 
			goto error_close_node;

		if (reiser4_node_items(node) == 0) {
			reiser4_bitmap_clear(bitmap, node->block->nr);
			repair_am_blk_free(am, node->block->nr);
			reiser4_node_close(node);
			stat->empty++;
			blk++;
			continue;
		}

		res = repair_tree_attach_node(am->repair->fs->tree, node);
		
		if (res < 0 && res != -ESTRUCT) {
			aal_error("Add missing pass failed to attach "
				  "the node (%llu) to the tree.",
				  (unsigned long long)blk);

			goto error_close_node;
		} else if (res == 0) {
			/* Has been inserted. */
			reiser4_bitmap_clear(bitmap, node->block->nr);
			repair_am_blk_used(am, node->block->nr);

			stat->by_node++;

			res = reiser4_node_trav(node, cb_layout, am);
			if (res) goto error_close_node;

		} else {
			/* uninsertable case - insert by item later. */
			reiser4_node_fini(node);
		}
		
		blk++;
	}

	return 0;

 error_close_node:
	reiser4_node_close(node);
	return res;
}

static errno_t repair_am_items_insert(repair_am_t *am, 
				      reiser4_bitmap_t *bitmap, 
				      stat_bitmap_t *stat)
{
	uint32_t count;
	reiser4_node_t *node;
	uint64_t total;
	errno_t res;
	blk_t blk;

	aal_assert("vpf-1285", am != NULL);
	aal_assert("vpf-1286", bitmap != NULL);
	aal_assert("vpf-1287", stat != NULL);
	
	total = reiser4_bitmap_marked(bitmap);
	blk = 0;
	/* Insert extents from the twigs/all items from leaves which 
	   are not in the tree yet item-by-item into the tree, overwrite
	   existent data which is in the tree already if needed. 
	   FIXME: overwriting should be done on the base of flush_id. */
	while ((blk = reiser4_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
		reiser4_place_t place;
		pos_t *pos = &place.pos;

		aal_assert("vpf-897", !reiser4_bitmap_test(am->bm_used, blk));

		node = reiser4_node_open(am->repair->fs->tree, blk);
		
		if (node == NULL) {
			aal_error("Add Missing pass failed to "
				  "open the node (%llu)",
				  (unsigned long long)blk);
			return -EINVAL;
		}

		count = reiser4_node_items(node);
		place.node = node;

		stat->by_item++;
		aal_gauge_set_value(am->gauge, stat->by_item * 100 / total);
		aal_gauge_touch(am->gauge);

		for (pos->item = 0; pos->item < count; pos->item++) {
			pos->unit = MAX_UINT32;

			if ((res = reiser4_place_fetch(&place))) {
				aal_error("Node (%llu), item (%u): "
					  "cannot open the item "
					  "place.",
					  (unsigned long long)blk,
					  pos->item);

				goto error_close_node;
			}

			res = repair_tree_insert(am->repair->fs->tree, &place,
						 cb_item_mark_region, am);
			
			if (res < 0) 
				goto error_close_node;
			
			if (res && place.plug->p.id.group == STAT_ITEM) {
				/* If insertion cannot be performed for the 
				   statdata item, descement file counter. */
				(*am->stat.files)--;
			}
		}

		reiser4_bitmap_clear(bitmap, node->block->nr);
		repair_am_blk_free(am, node->block->nr);
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
	reiser4_bitmap_t *bitmap;
	stat_bitmap_t stat;
	uint32_t bnum;
	errno_t res;
	
	aal_assert("vpf-595", am != NULL);
	aal_assert("vpf-846", am->repair != NULL);
	aal_assert("vpf-847", am->repair->fs != NULL);
	aal_assert("vpf-848", am->bm_twig != NULL);
	aal_assert("vpf-849", am->bm_leaf != NULL);
	
	aal_mess("INSERTING UNCONNECTED NODES");
	am->gauge = aal_gauge_create(aux_gauge_handlers[GT_PROGRESS], 
				     NULL, NULL, 500, NULL);
	time(&am->stat.time);
	
	/* 2 loops - 1 for twigs, another for leaves. */
	for (bnum = 0; bnum < 2; bnum++) {
		if (bnum) {
			bitmap = am->bm_leaf;
			aal_gauge_rename(am->gauge, "3. Leaves: ");
		} else {
			bitmap = am->bm_twig;
			aal_gauge_rename(am->gauge, "1. Twigs: ");
		}
		
		/* Debugging of item coping. */
		if (am->repair->flags & (1 << REPAIR_DEBUG)) 
			goto debug;
		
		aal_memset(&stat, 0, sizeof(stat));
		
		aal_gauge_set_value(am->gauge, 0);
		aal_gauge_touch(am->gauge);
		
		if ((res = repair_am_nodes_insert(am, bitmap, &stat)))
			goto error;
		
		if (bnum) {
			am->stat.read_leaves = stat.read;
			am->stat.by_leaf = stat.by_node;
		} else {
			am->stat.read_twigs = stat.read;
			am->stat.by_twig = stat.by_node;
			am->stat.empty = stat.empty;
		}
	
		aal_gauge_done(am->gauge);
	debug:
		if (bnum) {
			aal_gauge_rename(am->gauge, "4. Leaves by item: ");
		} else {
			aal_gauge_rename(am->gauge, "2. Twigs by item: ");
		} 
		
		aal_memset(&stat, 0, sizeof(stat));
		
		aal_gauge_set_value(am->gauge, 0);
		aal_gauge_touch(am->gauge);

		if ((res = repair_am_items_insert(am, bitmap, &stat)))
			goto error;

		if (bnum)
			am->stat.by_item_leaves = stat.by_item;
		else
			am->stat.by_item_twigs = stat.by_item;
		
		aal_gauge_done(am->gauge);
	}
	
	aal_gauge_free(am->gauge);
	repair_add_missing_update(am);
	reiser4_fs_sync(am->repair->fs);
	
	return 0;

 error:
	aal_gauge_done(am->gauge);
	aal_gauge_free(am->gauge);
	reiser4_fs_sync(am->repair->fs);
	return res;
}

