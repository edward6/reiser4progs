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
    repair_am_t *am;
    reiser4_coord_t neigh;    
    reiser4_key_t dkey;
    uint16_t items, units;
 
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
    
    if (coord->pos.item < items && coord->pos.unit < units) {
	if (reiser4_item_key(coord)) 
	    return -1;

	aal_memcpy(&dkey, &coord->entity.key, sizeof(dkey));
	
	/* if true - the most right key of what is going to be inserted is 
	 * greater then the key of the pos we found. */
	if (reiser4_key_compare(&am->max_real_key, &dkey) >= 0)
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
    }

    if (neigh.node) {
	if (repair_node_child_max_real_key(&neigh, &dkey))
	    return -1;

	/* If true - the most right key of the previous item is greater then 
	 * the first key of what is going to be inserted. */
	if (reiser4_key_compare(&dkey, &hint->key) >= 0)
	    return 1;
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
		    fetch, &coord->entity, coord->pos.unit, &ptr, 1))
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
    aal_assert("vpf-632", am->bm_uninserable != NULL, return -1);
    
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
    reiser4_level_t stop = {TWIG_LEVEL, TWIG_LEVEL};
    repair_am_t *am;
    uint32_t items;
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

    blk = 0;

    while ((blk = aux_bitmap_find_marked(am->bm_leaf, blk)) != INVAL_BLK) {
	if ((node = repair_node_open(rd->fs->format, blk)) == NULL) {
	    aal_exception_fatal("Add Missing pass failed to open the node "
		"(%llu)", blk);
	    return -1;
	}

	level = plugin_call(goto error_node_free, 
	    node->entity->plugin->node_ops, get_level, node->entity);

	/* This block must contain twig. */
	aal_assert("vpf-544", level == LEAF_LEVEL, goto error_node_free);

	pos->item = reiser4_node_items(node) - 1;
	pos->unit = ~0ul;

	if (reiser4_coord_realize(&coord)) {
	    aal_exception_error("Node (%llu): Failed to open the item (%llu).",
		node->blk, pos->item);
	    goto error_node_free;
	}
	    
	if (reiser4_item_max_real_key(&coord, &am->max_real_key))
	    goto error_node_free;

	res = reiser4_tree_attach(tree, node);

	if (res < 0) {
	    aal_exception_bug("Add missing pass failed to attach the leaf "
		"(%llu) to the tree.", blk);
	    goto error_node_free;
	} else if (res > 0) {
	    /* uninsertable. */
	}
	
	res = -1;
	
	aux_bitmap_clear(am->bm_leaf, node->blk);
	reiser4_node_close(node);
    }
    
    while ((blk = aux_bitmap_find_marked(am->bm_twig, blk)) != INVAL_BLK) {
	if ((node = repair_node_open(rd->fs->format, blk)) == NULL) {
	    aal_exception_fatal("Add Missing pass failed to open the node "
		"(%llu)", blk);
	    return -1;
	}

	level = plugin_call(goto error_node_free, 
	    node->entity->plugin->node_ops, get_level, node->entity);

	/* This block must contain twig. */
	aal_assert("vpf-544", level == TWIG_LEVEL, goto error_node_free);

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

	    aal_assert("vpf-637", reiser4_item_extent(&coord), goto error_node_free);

	    aal_memset(&hint, 0, sizeof(hint));

	    hint.plugin = reiser4_item_plugin(&coord);
	    hint.data = reiser4_item_body(&coord);
	    hint.len = reiser4_item_len(&coord);

	    if (reiser4_item_key(&coord)) {
		aal_exception_error("Node (%llu), item (%u), unit (%u): "
		    "failed to get the item key.", node->blk, pos->item, 
		    pos->unit);
		goto error_node_free;
	    }

	    aal_memcpy(&hint.key, &coord.entity.key, sizeof(hint.key));
		
	    if (reiser4_item_max_real_key(&coord, &am->max_real_key))
		goto error_node_free;
		
	    res = reiser4_tree_lookup(tree, &hint.key, &stop, &found_coord);
    	
	    if (res < 0) {
		aal_stream_t stream = EMPTY_STREAM;
		    
		reiser4_key_print(&hint.key, &stream);
		    
		aal_exception_bug("Add missing pass failed to lookup the "
		    "key %s in the tree.", stream.data);
		aal_stream_fini(&stream);
		    
		goto error_node_free;
	    } else if (res > 0) {
		/* Uninsertable. */
	    } else {
		if ((res = reiser4_tree_insert(tree, &found_coord, &hint))) {
		    aal_exception_error("Can't insert node (%llu) to the tree.", 
			node->blk);
		    goto error_node_free;
		}
	    }
		
	    res = -1;
	}

	aux_bitmap_clear(am->bm_twig, node->blk);
	reiser4_node_close(node);
    }
    
    tree->traps.preinsert = NULL;
    tree->traps.pstinsert = NULL;
    tree->traps.data = NULL;

    return 0;

error_node_free:
    reiser4_node_close(node);

    return res;
}

