/*
    repair/add_missing.c -- the common methods for insertion leaves and extent 
    item from twigs unconnected from the tree.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/add_missing.h>

/* Callback for item_ops.layout method to mark all the blocks, items points to, 
 * in the allocator. */
static errno_t callback_item_mark_region(void *object, uint64_t start, 
    uint64_t count, void *data)
{
    item_entity_t *item = (item_entity_t *)object;
    reiser4_alloc_t *alloc = (reiser4_alloc_t *)data;
    
    aal_assert("vpf-735", data != NULL);
    
    if (start != 0) {
	reiser4_alloc_permit(alloc, start, count);
	reiser4_alloc_occupy(alloc, start, count);
    }

    return 0;
}

/* Callback for traverse through all items of the node. Calls for the item, 
 * determined by place, layout method, if it is not the branch and has 
 * pointers to some blocks. */
static errno_t callback_layout(reiser4_place_t *place, void *data) {
    aal_assert("vpf-649", place != NULL);
    aal_assert("vpf-748", reiser4_item_data(place->item.plugin));

    if (!place->item.plugin->item_ops.layout)
	return 0;
	
    /* All these blocks should not be used in the allocator and should be 
     * forbidden for allocation. Check it somehow first. */
    return place->item.plugin->item_ops.layout(&place->item, 
	callback_item_mark_region, data);
}

static void repair_add_missing_setup(repair_am_t *am) {
    aal_assert("vpf-887", am != NULL);
    
    aal_memset(am->progress, 0, sizeof(*am->progress));
    am->progress->type = PROGRESS_RATE;
    am->progress->title = "***** AddMissing Pass: inserting unconnected nodes "
	"into the tree.";
    time(&am->stat.time);
}

static void repair_add_missing_update(repair_am_t *am) {
    aal_stream_t stream;
    char *time_str;

    aal_assert("vpf-886", am != NULL);

    if (!am->progress_handler)
	return;
	
    aal_stream_init(&stream);
    
    aal_stream_format(&stream, "\tTwigs: read %llu, inserted %llu, by items "
	"%llu\n", am->stat.read_twigs, am->stat.by_twig, 
	am->stat.by_item_twigs);
	    
    aal_stream_format(&stream, "\tLeaves: read %llu, inserted %llu, by items "
	"%llu\n", am->stat.read_leaves, am->stat.by_leaf, 
	am->stat.by_item_leaves);

    time_str = ctime(&am->stat.time);
    time_str[aal_strlen(time_str) - 1] = '\0';
    aal_stream_format(&stream, "\tTime interval: %s - ", time_str);
    time(&am->stat.time);
    time_str = ctime(&am->stat.time);
    time_str[aal_strlen(time_str) - 1] = '\0';
    aal_stream_format(&stream, time_str);

    am->progress->state = PROGRESS_STAT;
    am->progress->text = (char *)stream.data;
    am->progress_handler(am->progress);
    
    aal_stream_fini(&stream);
}

/* The pass inself, adds all the data which are not in the tree yet and which 
 * were found on the partition during the previous passes. */
errno_t repair_add_missing(repair_am_t *am) {
    repair_progress_t progress;
    reiser4_place_t place;
    pos_t *pos = &place.pos;
    reiser4_node_t *node;
    aux_bitmap_t *bitmap;
    uint32_t items, count, i;
    errno_t res;
    blk_t blk;
    
    aal_assert("vpf-595", am != NULL);
    aal_assert("vpf-846", am->repair != NULL);
    aal_assert("vpf-847", am->repair->fs != NULL);
    aal_assert("vpf-848", am->bm_twig != NULL);
    aal_assert("vpf-849", am->bm_leaf != NULL);
    
    am->progress = &progress;
    
    repair_add_missing_setup(am);
   
    /* 2 loops - 1 for twigs, another for leaves. */
    for (i = 0; i < 2; i++) {
	blk = 0;
	
	if (i == 0) {
	    bitmap = am->bm_twig;
	    am->progress->text = "Inserting unconnected twigs: ";
	    am->progress->u.rate.total = aux_bitmap_marked(am->bm_twig);
	} else {
	    bitmap = am->bm_leaf;
	    am->progress->text = "Inserting unconnected leaves: ";
	    am->progress->u.rate.total = aux_bitmap_marked(am->bm_leaf);
	}
	
	am->progress->state = PROGRESS_START;
	if (am->progress_handler)
	    am->progress_handler(am->progress);
	
	am->progress->state = PROGRESS_UPDATE;
 
       	/* Try to insert the whole twig/leaf at once. If found twig/leaf could 
	 * be split into 2 twigs/leaves and the wanted wtig/leaf fits between 
	 * them w/out problem - it will be done instead of following item by 
	 * item insertion. */
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
	    node = repair_node_open(am->repair->fs, blk);
	    
	    if (i == 0)
		am->stat.read_twigs++;
	    else
		am->stat.read_leaves++;
	    
	    if (am->progress_handler)
		am->progress_handler(am->progress);
	    
	    if (node == NULL) {
		aal_exception_fatal("Add Missing pass failed to open the node "
		    "(%llu)", blk);
		return -EINVAL;
	    }

	    /* Prepare the node for insertion - Remove all metadata items. */
	    place.node = node;
	    pos->unit = ~0ul;
	    count = reiser4_node_items(node);
	    for (pos->item = 0; pos->item < count; pos->item++) {
		if ((res = reiser4_place_realize(&place))) {
		    aal_exception_error("Node (%llu), item (%u): failed to open"
			" the item.", node->blk, pos->item);
		    goto error_node_close;
		}
		
		/* If an item does not contain data (only metadata),remove it.*/
		/* FIXME: Just a reminder - here should be deleted all metadata
		* info from items, only user data should be left. For now, items
		* contain data xor metadata, not both. */
		if (!reiser4_item_data(place.item.plugin)) {
		    if ((res = reiser4_node_remove(place.node, pos, 1))) {
			aal_exception_error("Node (%llu), item (%u): failed to "
			    "remove the item.", node->blk, pos->item);
			goto error_node_close;
		    }
		    
		    reiser4_node_mkdirty(place.node);
		    pos->item--;
		    count = reiser4_node_items(node);
		}
	    }

	    if (reiser4_node_items(node) == 0) {
		reiser4_node_mkclean(place.node);
		aux_bitmap_clear(bitmap, node->blk);
		reiser4_alloc_permit(am->repair->fs->alloc, node->blk, 1);
		goto find_next;
	    }
		
	    res = repair_tree_attach(am->repair->fs->tree, node);

	    if (res < 0 && res != -ESTRUCT) {
		aal_exception_bug("Add missing pass failed to attach the %s "
		    "(%llu) to the tree.", blk, i == 0 ? "twig" : "leaf");
		goto error_node_close;
	    } else if (res == 0) {
		/* Has been inserted. */
		aux_bitmap_clear(bitmap, node->blk);
		reiser4_alloc_permit(am->repair->fs->alloc, node->blk, 1);
		reiser4_alloc_occupy(am->repair->fs->alloc, node->blk, 1);

		if (i == 0)
		    am->stat.by_twig++;
		else
		    am->stat.by_leaf++;
		
		res = repair_node_traverse(node, callback_layout, 
		    am->repair->fs->alloc);

		if (res)
		    goto error_node_close;
		
		blk++;
		continue;
	    } /* if res > 0 - uninsertable case - insert by items later. */
	
	find_next:
	    reiser4_node_close(node);
	    blk++;
	}

	am->progress->state = PROGRESS_END;
	am->progress_handler(am->progress);
	
	blk = 0;
	if (i == 0) {
	    am->progress->text = "Inserting unconnected leaves item-by-item: ";
	    am->progress->u.rate.total = aux_bitmap_marked(am->bm_twig);
	    am->progress->u.rate.done = 0;
	} else {
	    am->progress->text = "Inserting unconnected twigs item-by-item: ";
	    am->progress->u.rate.total = aux_bitmap_marked(am->bm_leaf);
	    am->progress->u.rate.done = 0;
	}
	
	am->progress->state = PROGRESS_START;
	am->progress_handler(am->progress);

	am->progress->state = PROGRESS_UPDATE;
	
	/* Insert extents from the twigs/all items from leaves which are not in 
	 * the tree yet item-by-item into the tree, overwrite existent data 
	 * which is in the tree already if needed. FIXME: overwriting should be 
	 * done on the base of flush_id. */
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
	    node = repair_node_open(am->repair->fs, blk);
	    
	    if (am->progress_handler)
		am->progress_handler(am->progress);
	    
	    if (node == NULL) {
		aal_exception_fatal("Add Missing pass failed to open the node "
		    "(%llu)", blk);
		return -EINVAL;
	    }

	    pos->unit = ~0ul;
	    items = reiser4_node_items(node);
	    place.node = node;

	    for (pos->item = 0; pos->item < items; pos->item++) {
		aal_assert("vpf-636", pos->unit == ~0ul);

		if ((res = reiser4_place_realize(&place))) {
		    aal_exception_error("Node (%llu), item (%u): cannot open "
			"the item place.", blk, pos->item);
		    
		    goto error_node_close;
		}
	 
		if ((res = repair_tree_insert(am->repair->fs->tree, &place)))
		    goto error_node_close;

		if (i == 0)
		    am->stat.by_item_twigs++;
		else
		    am->stat.by_item_leaves++;
		
		if ((res = callback_layout(&place, am->repair->fs->alloc)))
		    goto error_node_close;
	    }
	
	    aux_bitmap_clear(bitmap, node->blk);
	    reiser4_alloc_permit(am->repair->fs->alloc, node->blk, 1);
	    reiser4_node_close(node);

	    blk++;
	}

	am->progress->state = PROGRESS_END;
	am->progress_handler(am->progress);
    }

    repair_add_missing(am);
    return 0;

error_node_close:
    reiser4_node_close(node);

error_progress_close:
    
    am->progress->state = PROGRESS_END;
    am->progress_handler(am->progress);

    return res;
}


