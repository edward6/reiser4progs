/*
    repair/add_missing.c -- the common methods for insertion leaves and extent 
    item from twigs unconnected from the tree.
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#include <repair/librepair.h>

/* Check the item order. */
static errno_t callback_preinsert(reiser4_coord_t *coord, 
    reiser4_item_hint_t *hint, void *data) 
{
    reiser4_coord_t neigh;
    reiser4_node_t *node;
    reiser4_ptr_hint_t ptr;
    reiser4_level_t stop;
    reiser4_key_t dkey;
    repair_am_t *am;
    uint16_t items, units;
    int lookup;
 
    aal_assert("vpf-596", coord != NULL, return -1);
    aal_assert("vpf-596", coord->node != NULL, return -1);
    aal_assert("vpf-597", hint != NULL, return -1);
    aal_assert("vpf-602", data != NULL, return -1);    

    am = repair_am((repair_data_t *)data);
    
    /* Check the found coord for validness for item insertion:
     * The ld_key/rd_key of the inserted item must be greater/less
     * then the rd_key/ld_key of the left/right neighboor. */

    if (reiser4_coord_realize(coord))
	return -1;
	
    items = reiser4_node_items(coord->node);    
    units = reiser4_item_units(coord);

    aal_assert("vpf-629", coord->pos.item < items || coord->pos.unit < units, 
	return -1);
    
    /* Coord point to some unit. */
    if ((coord->pos.item < items) && 
	(coord->pos.unit < units || coord->pos.unit == ~0ul)) 
    {
	if (reiser4_item_get_key(coord, NULL)) 
	    return -1;

	/* if true - the most right key of what is going to be inserted is 
	 * greater then the key of the pos we found. */
	if (reiser4_key_compare(&am->max_real_key, &coord->entity.key) >= 0)
	    return 1;
    }
    
    neigh = *coord;
 
    if (coord->pos.unit != 0 && coord->pos.unit != ~0ul) {
	neigh.pos.unit--;
    } else {
	if (coord->pos.item != 0) {
	    neigh.pos.item--;
	    neigh.pos.unit = ~0ul;
	} else {
	    if ((neigh.node = reiser4_node_left(coord->node))) {
		/* left neighbour exists. */
		neigh.pos.item = reiser4_node_items(neigh.node);
		neigh.pos.unit = ~0ul;
	    }
	}
	if (neigh.node && reiser4_coord_realize(&neigh)) {
	    aal_exception_error("Node (%llu): Failed to open the item (%llu).",
		neigh.node->blk, neigh.pos.item);
	    return -1;
	}
	units = reiser4_item_units(&neigh);
	
	if (neigh.pos.unit == ~0ul && units > 1) 
	    neigh.pos.unit = units - 1;
    }
 
    if (!neigh.node) 
	return 0;
    
    if (repair_node_child_max_real_key(&neigh, &dkey))
	return -1;

    /* If true - the most right key of the previous item is greater then 
     * the first key of what is going to be inserted. */
    if (reiser4_key_compare(&dkey, &hint->key) < 0)
	return 0;
    
    /* The most right key of the previous internal unit is greater then the 
     * first key of what is going to be inserted. Try to split the node 
     * pointed by the previous internal unit so as the whole node specified 
     * by the hint could be inserted as the whole. */

    /* It is possible to perform it for nodeptr only. */
    if (!reiser4_item_nodeptr(&neigh))
	return 1;
    
    if (plugin_call(return -1, neigh.entity.plugin->item_ops, fetch, 
	&neigh.entity, &ptr, neigh.pos.unit, 1) != 1 || ptr.ptr == INVAL_BLK)
	return -1;
    
    if (!(neigh.node = reiser4_node_open(neigh.node->device, ptr.ptr))) 
	return -1;
    
    if ((lookup = reiser4_node_lookup(neigh.node, &hint->key, 
	&neigh.pos)) == -1)
	return -1;

    aal_assert("vpf-641", neigh.pos.item < reiser4_node_items(neigh.node), 
	return -1);
	
    if (reiser4_coord_realize(&neigh)) {
	aal_exception_error("Node (%llu), item (%u): Can't open the item.", 
	    neigh.node->blk, &neigh.pos.item);
	return -1;
    }

    aal_assert("vpf-642", neigh.pos.unit < reiser4_item_units(&neigh), 
	return -1);
    
    if (reiser4_item_get_key(&neigh, NULL)) {
	aal_exception_error("Node (%llu), item (%u): Can't get the item key.", 
	    neigh.node->blk, &neigh.pos.item);
	return -1;
    }

    /* If the key of the coord is greater then the max key of what is going to be 
     * inserted, then the whole node could be inserted here. Split the neigh node 
     * and attach the node there. */
    if (reiser4_key_compare(&am->max_real_key, &neigh.entity.key) >= 0)
	return 1;

    if ((node = reiser4_tree_allocate(coord->node->tree, 
	reiser4_node_level(neigh.node))))
    {
	aal_exception_error("Tree failed to allocate a new node.");
	return -1;
    }
    
    /* set flush_id */
    reiser4_node_set_flush_stamp(node, reiser4_node_get_flush_stamp(neigh.node));
    
    if (reiser4_tree_shift(coord->node->tree, &neigh, node, SF_RIGHT)) {
	aal_exception_error("Tree failed to shift into a newly allocated node.");
	return -1;
    }

    aal_assert("vpf-640", reiser4_node_items(node) != 0, return -1);

    if (reiser4_tree_attach(coord->node->tree, node)) {
	aal_exception_error("Tree failed to attach a newly allocated node to "
	    "the tree.");
	return -1;
    }

    /* Parent got changed after splitting, lookup the wanted key again to have 
     * coord updated. */
    stop.top = stop.bottom = reiser4_node_level(coord->node) + 1;
    if (reiser4_tree_lookup(coord->node->tree, &hint->key, &stop, coord)) {
	aal_exception_error("Lookup failed to find the proper place for item "
	    "insertion.");
	return -1;
    }
 
    return 0;
}

/* Mark/clear needed blocks in proper bitmaps. */
static errno_t callback_pstinsert(reiser4_coord_t *coord, 
    reiser4_item_hint_t *hint, void *data) 
{   
    uint32_t i, units;
    repair_am_t *am;
    
    aal_assert("vpf-598", coord != NULL, return -1);
    aal_assert("vpf-599", hint != NULL, return -1);
    aal_assert("vpf-600", hint->plugin != NULL, return -1);
    aal_assert("vpf-601", data != NULL, return -1);

    am = repair_am((repair_data_t *)data);

    if (hint->plugin->h.group == NODEPTR_ITEM) {
	reiser4_ptr_hint_t *ptr;
	ptr = (reiser4_ptr_hint_t *)hint->hint;
	aux_bitmap_mark_range(am->bm_used, ptr->ptr, ptr->width);
    } else if (hint->plugin->h.group == EXTENT_ITEM) {
	reiser4_ptr_hint_t ptr;
	/* Extent item. */
	/* Mark blocks pointed by extent as used. */
	if (reiser4_coord_realize(coord))
	    return -1;

	if (coord->pos.unit == ~0ul) {
	    units = reiser4_item_units(coord);

	    for (i = 0; i < units; i++) {
		if (plugin_call(return -1, coord->entity.plugin->item_ops,
		    fetch, &coord->entity, &ptr, coord->pos.unit, 1) != 1)
		    return -1;

		aux_bitmap_mark_range(am->bm_used, ptr.ptr, ptr.width);
	    }
	} else {
	    aal_exception_bug("Unexpected error. All extents should be inserted "
		"by items, not by units.");
	    return -1;
	}
    } else	
	return -1;

    return 0;
}

static errno_t repair_am_setup(repair_data_t *rd) {
    repair_am_t *am;
    
    aal_assert("vpf-594", rd != NULL, return -1);
    aal_assert("vpf-618", rd->fs != NULL, return -1);
    aal_assert("vpf-619", rd->fs->format != NULL, return -1);
    
    am = repair_am(rd);

    aal_assert("vpf-630", am->bm_used != NULL, return -1);
    aal_assert("vpf-631", am->bm_leaf != NULL, return -1);
    aal_assert("vpf-631", am->bm_twig != NULL, return -1);
    
    if (reiser4_format_get_root(rd->fs->format) == INVAL_BLK) {
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
    reiser4_coord_t coord, found_coord;
    reiser4_pos_t *pos = &coord.pos;
    reiser4_item_hint_t hint;
    reiser4_tree_t *tree;
    reiser4_node_t *node;
    reiser4_level_t stop[2] = {	{TWIG_LEVEL, TWIG_LEVEL},
				{LEAF_LEVEL, LEAF_LEVEL} };
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

    tree->traps.preinsert = callback_preinsert;
    tree->traps.pstinsert = callback_pstinsert;
    tree->traps.data = rd;

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

	    level = plugin_call(goto error_node_free, 
		node->entity->plugin->node_ops, get_level, node->entity);

	    /* This block must contain twig/leaf. */
	    aal_assert("vpf-638", level == (i == 0 ? TWIG_LEVEL : LEAF_LEVEL), 
		goto error_node_free);

	    pos->item = reiser4_node_items(node) - 1;
	    pos->unit = ~0ul;

	    if (reiser4_coord_realize(&coord)) {
		aal_exception_error("Node (%llu): Failed to open the item "
		    " (%llu).", node->blk, pos->item);
		goto error_node_free;
	    }

	    if (reiser4_item_max_real_key(&coord, &am->max_real_key))
		goto error_node_free;

	    res = reiser4_tree_attach(tree, node);

	    if (res < 0) {
		aal_exception_bug("Add missing pass failed to attach the %s "
		    "(%llu) to the tree.", blk, i == 0 ? "twig" : "leaf");
		goto error_node_free;
	    } else if (res == 0) {
		/* Has been inserted. */
		aux_bitmap_clear(bitmap, node->blk);
	    }
	
	    res = -1;
	    reiser4_node_close(node);
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

	    level = plugin_call(goto error_node_free, 
		node->entity->plugin->node_ops, get_level, node->entity);

	    /* This block must contain twig/leaf. */
	    aal_assert("vpf-544", level == (i == 0 ? TWIG_LEVEL : LEAF_LEVEL), 
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

		aal_memset(&hint, 0, sizeof(hint));

		hint.plugin = reiser4_item_plugin(&coord);
		hint.data = reiser4_item_body(&coord);
		hint.len = reiser4_item_len(&coord);

		if (reiser4_item_get_key(&coord, &hint.key)) {
		    aal_exception_error("Node (%llu), item (%u), unit (%u): "
			"failed to get the item key.", node->blk, pos->item, 
			pos->unit);
		    goto error_node_free;
		}

		if (reiser4_item_max_real_key(&coord, &am->max_real_key))
		    goto error_node_free;
		
		res = reiser4_tree_lookup(tree, &hint.key, &stop[i], 
		    &found_coord);

		if (res < 0) {
		    aal_stream_t stream = EMPTY_STREAM;
		    
		    reiser4_key_print(&hint.key, &stream);
		    
		    aal_exception_bug("Add missing pass failed to lookup the "
			"key %s in the tree.", stream.data);
		    aal_stream_fini(&stream);
		    
		    goto error_node_free;
		} else if (res > 0) {
		    /* Key exists in the tree already. Prepare overwrite. */
		} else {
		    if ((res = reiser4_tree_insert(tree, &found_coord, &hint))) 
		    {
			aal_exception_error("Add missing pass failed to insert "
			    "the item (%u) from the node (%llu) into the tree.",
			    node->blk);
			goto error_node_free;
		    }
		}
		
		res = -1;
	    }
	
	    aux_bitmap_clear(am->bm_twig, node->blk);
	    reiser4_node_close(node);
	}
	
	bitmap = am->bm_leaf;
    }

    tree->traps.preinsert = NULL;
    tree->traps.pstinsert = NULL;
    tree->traps.data = NULL;

    return 0;

error_node_free:
    reiser4_node_close(node);

    return res;
}

