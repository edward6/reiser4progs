/*
    repair/add_missing.c -- the common methods for insertion leaves and extent 
    item from twigs unconnected from the tree.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Callback for item_ops.layout method to mark all the blocks, items points to, 
 * in the allocator. */
static errno_t callback_item_mark_region(item_entity_t *item, uint64_t start, 
    uint64_t count, void *data)
{
    reiser4_alloc_t *alloc = (reiser4_alloc_t *)data;
    
    aal_assert("vpf-735", data != NULL);
    
    if (start != 0) {
	reiser4_alloc_permit(alloc, start, count);
	reiser4_alloc_occupy_region(alloc, start, count);
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

/* If a fatal error occured, release evth, what was allocated by this moment 
 * - not only on this pass, smth was allocated on some previous one. */
static void repair_add_missing_release(repair_data_t *rd) {
    aal_assert("vpf-739", rd != NULL);

    if (repair_am(rd)->bm_twig)
	aux_bitmap_close(repair_am(rd)->bm_twig);
    if (repair_am(rd)->bm_leaf)
	aux_bitmap_close(repair_am(rd)->bm_leaf);
    if (repair_am(rd)->tree)
	reiser4_tree_close(repair_am(rd)->tree);
}

/* Setup the pass to be performed - open or create the tree. */
static errno_t repair_add_missing_setup(repair_data_t *rd) {
    aal_assert("vpf-594", rd != NULL);
    aal_assert("vpf-618", rd->fs != NULL);
    aal_assert("vpf-619", rd->fs->format != NULL);
    
    if (reiser4_format_get_root(rd->fs->format) == INVAL_BLK) {
	/* Trere is no any tree yet.  */
	if (!(rd->fs->tree = reiser4_tree_init(rd->fs))) {
	    aal_exception_fatal("Failed to create the tree of the fs.");
	    goto error;
	}
    } else {
	/* There is some tree already. */
	if (!(rd->fs->tree = reiser4_tree_init(rd->fs))) {
	    aal_exception_fatal("Failed to open the tree of the fs.");
	    goto error;
	}
    }
    
    return 0;

error:
    repair_add_missing_release(rd);
    
    return -1;
}

/* The pass inself, adds all the data which are not in the tree yet and which 
 * were found on the partition during the previous passes. */
errno_t repair_add_missing_pass(repair_data_t *rd) {
    reiser4_place_t place;
    rpos_t *pos = &place.pos;
    reiser4_tree_t *tree;
    reiser4_node_t *node;
    aux_bitmap_t *bitmap;
    repair_am_t *am;
    uint32_t items, i;
    errno_t res = -1;
    blk_t blk;
    
    aal_assert("vpf-595", rd != NULL);

    am = repair_am(rd);

    if (repair_add_missing_setup(rd))
	return -1;
    
    tree = rd->fs->tree;    

    bitmap = am->bm_twig;
    
    /* 2 loops - 1 for twigs, another for leaves. */
    for (i = 0; i < 2; i++) {
	blk = 0;

       	/* Try to insert the whole twig/leaf at once. If found twig/leaf could 
	 * be split into 2 twigs/leaves and the wanted wtig/leaf fits between 
	 * them w/out problem - it will be done instead of following item by 
	 * item insertion. */
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
	    node = repair_node_open(rd->fs, blk);
	    if (node == NULL) {
		aal_exception_fatal("Add Missing pass failed to open the node "
		    "(%llu)", blk);
		goto error;
	    }

	    res = repair_tree_attach(tree, node);

	    if (res < 0) {
		aal_exception_bug("Add missing pass failed to attach the %s "
		    "(%llu) to the tree.", blk, i == 0 ? "twig" : "leaf");
		goto error_node_free;
	    } else if (res == 0) {
		/* Has been inserted. */
		aux_bitmap_clear(bitmap, node->blk);
		reiser4_alloc_permit(rd->fs->alloc, node->blk, 1);
		reiser4_alloc_occupy_region(rd->fs->alloc, node->blk, 1);

		if (repair_node_traverse(node, callback_layout, rd->fs->alloc))
		    goto error_node_free;

	    } /* if res > 0 - uninsertable case - insert by items later. */
	
	    res = -1;
	    reiser4_node_close(node);
	    blk++;
	}

	blk = 0;

	/* Insert extents from the twigs/all items from leaves which are not in 
	 * the tree yet item-by-item into the tree, overwrite existent data 
	 * which is in the tree already if needed. FIXME: overwriting should be 
	 * done on the base of flush_id. */    
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
	    node = repair_node_open(rd->fs, blk);
	    if (node == NULL) {
		aal_exception_fatal("Add Missing pass failed to open the node "
		    "(%llu)", blk);
		goto error;
	    }

	    pos->unit = ~0ul;
	    items = reiser4_node_items(node);
	    place.node = node;

	    for (pos->item = 0; pos->item < items; pos->item++) {
		aal_assert("vpf-636", pos->unit == ~0ul);

		if (reiser4_place_realize(&place)) {
		    aal_exception_error("Node (%llu), item (%u): cannot open "
			"the item place.", blk, pos->item);
		    
		    goto error_node_free;
		}
	 
		if (repair_tree_insert(tree, &place))
		    goto error_node_free;

		if (callback_layout(&place, rd->fs->alloc))
		    goto error_node_free;
	    }
	
	    aux_bitmap_clear(bitmap, node->blk);
	    reiser4_alloc_permit(rd->fs->alloc, node->blk, 1);
	    reiser4_node_close(node);

	    blk++;
	}
	
	bitmap = am->bm_leaf;
    }

    return 0;

error_node_free:
    reiser4_node_close(node);

error:
    repair_add_missing_release(rd);
    
    return -1;
}


