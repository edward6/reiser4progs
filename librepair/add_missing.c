/*
    repair/add_missing.c -- the common methods for insertion leaves and extent 
    item from twigs unconnected from the tree.
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#include <repair/librepair.h>

static errno_t callback_extent_used(reiser4_coord_t *coord, void *data) {
    reiser4_ptr_hint_t ptr;
    rpos_t *pos;
    repair_am_t *am = (repair_am_t *)data;
    uint32_t units;

    aal_assert("vpf-649", coord != NULL, return -1);
    aal_assert("vpf-651", am != NULL, return -1);
    aal_assert("vpf-650", reiser4_item_extent(coord), return -1);

    pos = &coord->pos;
    units = reiser4_item_units(coord);

    for (pos->unit = 0; pos->unit < units; pos->unit++) {
	if (plugin_call(coord->item.plugin->item_ops,
	    read, &coord->item, &ptr, coord->pos.unit, 1) != 1)
	    return -1;

	/* All these blocks should not be used in the allocator and should be 
	 * forbidden for allocation. Check it somehow first. */
	aux_bitmap_mark_region(am->bm_used, ptr.ptr, ptr.width);	
    }
    
    return 0;
}

static errno_t repair_am_setup(repair_data_t *rd) {
    aal_assert("vpf-594", rd != NULL, return -1);
    aal_assert("vpf-618", rd->fs != NULL, return -1);
    aal_assert("vpf-619", rd->fs->format != NULL, return -1);
    
    if (reiser4_format_get_root(rd->fs->format) == INVAL_BLK) {
	/* Trere is no any tree yet.  */
	if (!(rd->fs->tree = reiser4_tree_create(rd->fs, rd->profile))) {
	    aal_exception_fatal("Failed to create the tree of the fs.");
	    return -1;
	}
    } else {
	/* There is some tree already. */
	if (!(rd->fs->tree = reiser4_tree_open(rd->fs))) {
	    aal_exception_fatal("Failed to open the tree of the fs.");
	    return -1;
	}
    }
    
    return 0;
}

errno_t repair_am_pass(repair_data_t *rd) {
    reiser4_coord_t coord;
    rpos_t *pos = &coord.pos;
    reiser4_tree_t *tree;
    reiser4_node_t *node;
    aux_bitmap_t *bitmap;
    repair_am_t *am;
    uint32_t items, i;
    uint8_t level;
    errno_t res = -1;
    blk_t blk;
    
    aal_assert("vpf-595", rd != NULL, return -1);

    am = repair_am(rd);

    if (repair_am_setup(rd))
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
	    if ((node = repair_node_open(rd->fs->format, blk)) == NULL) {
		aal_exception_fatal("Add Missing pass failed to open the node "
		    "(%llu)", blk);
		return -1;
	    }

	    level = reiser4_node_level(node); 

	    /* This block must contain twig/leaf. */
	    aal_assert("vpf-638", level == (i == 0 ? TWIG_LEVEL : LEAF_LEVEL), 
		goto error_node_free);

	    res = repair_tree_attach(tree, node);

	    if (res < 0) {
		aal_exception_bug("Add missing pass failed to attach the %s "
		    "(%llu) to the tree.", blk, i == 0 ? "twig" : "leaf");
		goto error_node_free;
	    } else if (res == 0) {
		/* Has been inserted. */
		aux_bitmap_clear(bitmap, node->blk);

		if (i) {
		    aux_bitmap_mark(am->bm_used, node->blk);
		} else {
		    if (repair_node_traverse(node, 1 << EXTENT_ITEM, 
			callback_extent_used, am))
		    goto error_node_free;
		}		
	    } /* if res > 0 - uninsertable case - insert by items later. */
	
	    res = -1;
	    reiser4_node_release(node);
	}

	blk = 0;

	/* Insert extents from the twigs/all items from leaves which are not in 
	 * the tree yet item-by-item into the tree, overwrite existent data 
	 * which is in the tree already if needed. FIXME: overwriting should be 
	 * done on the base of flush_id. */
    
	while ((blk = aux_bitmap_find_marked(bitmap, blk)) != INVAL_BLK) {
	    if ((node = repair_node_open(rd->fs->format, blk)) == NULL) {
		aal_exception_fatal("Add Missing pass failed to open the node "
		    "(%llu)", blk);
		return -1;
	    }

	    level = reiser4_node_level(node); 

	    /* This block must contain twig/leaf. */
	    aal_assert("vpf-709", level == (i == 0 ? TWIG_LEVEL : LEAF_LEVEL), 
		goto error_node_free);

	    pos->unit = ~0ul;
	    items = reiser4_node_items(node);
	    coord.node = node;

	    for (pos->item = 0; pos->item < items; pos->item++) {
		aal_assert("vpf-636", pos->unit == ~0ul, goto error_node_free);

		if (reiser4_coord_realize(&coord)) {
		    aal_exception_error("Node (%llu), item (%u): cannot open "
			"the item coord.", blk, pos->item);
		    
		    goto error_node_free;
		}
	 
		if (i == 0) {
		    aal_assert("vpf-637", reiser4_item_extent(&coord), 
			goto error_node_free);
		}


		if (repair_tree_insert(tree, &coord))
		    goto error_node_free;

		if (reiser4_item_extent(&coord)) {
		    if (callback_extent_used(&coord, am))
			return -1;
		}
	    }
	
	    aux_bitmap_clear(bitmap, node->blk);
	    reiser4_node_release(node);
	}
	
	bitmap = am->bm_leaf;
    }

    return 0;

error_node_free:
    reiser4_node_release(node);

    return res;
}

