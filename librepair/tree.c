/*
    tree.c -- repair/tree.c -- tree auxiliary code.
  
    Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Get the max real key existed in the tree. Go down through all right-most 
 * child to get it. */
static errno_t repair_tree_max_real_key(reiser4_node_t *node, 
    reiser4_key_t *key) 
{
    reiser4_coord_t coord;
    reiser4_node_t *child;
    errno_t res;

    aal_assert("vpf-614", node != NULL, return -1);
    aal_assert("vpf-615", key != NULL, return -1);

    coord.node = node;
    coord.pos.item = reiser4_node_items(node) - 1;
    coord.pos.unit = ~0ul;

    if (reiser4_coord_realize(&coord)) {
	aal_exception_error("Node (%llu): Failed to open the item (%u).",
	    node->blk, coord.pos.item);
	return -1;
    }
    
    if (reiser4_item_nodeptr(&coord)) {
	item_entity_t *item = &coord.entity;
	reiser4_ptr_hint_t ptr;

	coord.pos.unit = reiser4_item_units(&coord);
	
	if (plugin_call(return -1, item->plugin->item_ops, fetch, item, 
	    &ptr, coord.pos.unit, 1) != 1 || ptr.ptr == INVAL_BLK)
	    return -1;

	if (!(child = reiser4_node_open(coord.node->device, ptr.ptr))) 
	    return -1;
	
	res = repair_tree_max_real_key(child, key);
	
	if (reiser4_node_close(child))
	    return -1;
    } else 
	res = reiser4_item_max_real_key(&coord, key);

    return res;
}

/* This function creates nodeptr item on the nase of 'node' and insert it to 
 * the tree. */
errno_t repair_tree_attach(reiser4_tree_t *tree, reiser4_node_t *node) {
    reiser4_coord_t coord;
    reiser4_item_hint_t hint;
    reiser4_ptr_hint_t ptr;
    reiser4_level_t stop = {LEAF_LEVEL, LEAF_LEVEL};
    reiser4_key_t rkey, key;
    errno_t res;
    uint32_t level;
    int lookup;   

    aal_assert("vpf-658", tree != NULL, return -1);
    aal_assert("vpf-659", node != NULL, return -1);

    /* Stop at the same level to be able to split the found node and insert 
     * the passed node between its parts. */
    level = reiser4_node_level(node);

    while (level >= reiser4_tree_height(tree))
	reiser4_tree_grow(tree);

    /* Preparing nodeptr item hint */
    aal_memset(&hint, 0, sizeof(hint));
    aal_memset(&ptr, 0, sizeof(ptr));

    reiser4_node_lkey(node, &hint.key);

    if ((lookup = reiser4_tree_lookup(tree, &hint.key, &stop, &coord)))
	return lookup;
	
    /* If coord points to a not existing position, move right a bit. */
    if (coord.pos.item < reiser4_node_items(coord.node)) {
	if (reiser4_coord_realize(&coord))
	    return -1;

	if (coord.pos.unit == reiser4_item_units(&coord)) {
	    coord.pos.item++; 
	    coord.pos.unit = ~0ul;
	} 
    }
    /* Key does not exist in the tree. Check the found position. Try to split 
     * the node to insert the whole node. */

    if (repair_item_key(&coord, &key))
	return -1;

    if (repair_tree_max_real_key(node, &rkey))
	return -1;
 
    if (reiser4_key_compare(&key, &rkey) >= 0)
	return 1;
    
    hint.hint = &ptr;
    ptr.ptr = node->blk;
    ptr.width = 1;

    hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE,
	tree->profile.nodeptr);

    if (!hint.plugin) {
	aal_exception_error("Can't find item plugin by its id 0x%x.",
	    tree->profile.nodeptr);
	return -1;
    }

    if ((res = reiser4_tree_insert(tree, &coord, &hint))) {
	aal_exception_error("Can't insert nodeptr item to the tree.");
	return res;
    }

    /* Setting needed links between nodes in the tree cashe. */
    if (reiser4_node_attach(coord.node, node)) {
	aal_exception_error("Can't attach the node %llu in tree cache.", 
	    node->blk);
	return -1;
    }

    return 0;
}

/* Insert the item with overwriting of existent in the tree items if needed. */
errno_t repair_tree_insert(reiser4_tree_t *tree, reiser4_coord_t *insert) {
    reiser4_level_t stop;
    reiser4_item_hint_t hint;
    reiser4_coord_t coord;
    reiser4_key_t rkey;
    errno_t res;
    uint32_t length;

    aal_assert("vpf-654", tree != NULL, return -1);
    aal_assert("vpf-655", insert != NULL, return -1);
    aal_assert("vpf-657", insert->node != NULL, return -1);
    aal_assert("vpf-656", insert->node->tree != NULL, return -1);

    aal_memset(&hint, 0, sizeof(hint));

    hint.plugin = reiser4_item_plugin(insert);
    hint.data = reiser4_item_body(insert);
    hint.len = length = reiser4_item_len(insert);

    aal_assert("vpf-668", hint.plugin->h.group != NODEPTR_ITEM, return -1);
    
    if (reiser4_item_get_key(insert, &hint.key)) {
	aal_exception_error("Node (%llu), item (%u), unit (%u): failed to get "
	    "the item key.", insert->node->blk, insert->pos.item,
	    insert->pos.unit);
	return -1;
    }

    stop.top = stop.bottom = LEAF_LEVEL;
 
    while (length) {
	if ((res = reiser4_tree_lookup(tree, &hint.key, &stop, &coord)) < 0)
	    return res;
	else if (res == 0) {
	    /* Start key does not exist in the tree. Prepare the insertion. */

	    if (repair_tree_max_real_key(coord.node, &rkey))
		return -1; 

	    /*
	    if ((res = repair_tree_fits(&coord, &hint.key, &rkey)))
		return res;
	    */
	    if (reiser4_tree_insert(tree, &coord, &hint)) {
		aal_exception_error("Node (%llu), unit(%u): Add missing"
		    " pass failed to insert the item to the tree.",
		    coord.node->blk, coord.pos.item);
		return -1;
	    }
	} else {
	    /* Prepare the overwriting. */
	    
	}
    }

    return 0;
}

/* 
 * Check the coord for key range insertion possibility. 
 * Split found node if it helps. 
 * 
 * FIXME: This method implies that it is run after lookup and the wanted key 
 * was not found. So start_key is not checked here.
 */
/*
static errno_t repair_tree_fits(reiser4_coord_t *coord,
    reiser4_key_t *start_key, reiser4_key_t *end_key)
{
    reiser4_key_t key;
    uint32_t items, units;    

    aal_assert("vpf-596", coord != NULL, return -1);
    aal_assert("vpf-660", coord->node != NULL, return -1);
    
    if (coord->pos.item == items) {
	if (repair_node_rd_key(coord->node, &key))
	    return -1;
    } else {
	if (reiser4_coord_realize(coord)) {
	    aal_exception_error("Node (%llu): failed to open the item on "
		"the parent node.", coord->node->blk);
	    return -1;
	}

	aal_assert("vpf-671", coord->pos.unit < reiser4_item_units(coord), 
	    return -1);
	
	if (reiser4_item_get_key(coord, &key)) {
	    aal_exception_error("Node (%llu): failed to get the item key "
		"by its coord.", coord->node->blk);	    
	    return -1;
	}
    }

    if (reiser4_key_compare(&key, end_key) >= 0)
	return 1;
    
    return 0;
}
*/

/*
 
static errno_t repair_tree_shift(reiser4_tree_t *tree, reiser4_coord_t *coord) {
    reiser4_node_t *node;
    uint32_t level;
    
    aal_assert("vpf-665", coord != NULL, return -1);
    aal_assert("vpf-666", coord->node != NULL, return -1);
    aal_assert("vpf-667", coord->node->tree != NULL, return -1);

    if ((coord->pos.item == 0 && coord->pos.unit == 0) || 
	(coord->pos.item == reiser4_node_items(coord->node)))
	return 0;

    // Insertable but split should be performed. 
    // FIXME: coord could be realised already. Optimise it later. 
    if (reiser4_coord_realize(coord))
	return -1;
		
    level = reiser4_node_level(coord->node);
    
    if ((node = reiser4_tree_allocate(tree, level)) == NULL) {
	aal_exception_error("Tree failed to allocate a new node.");
	return -1;
    }

    // set flush_id 
    reiser4_node_set_flush_stamp(node, 
	reiser4_node_get_flush_stamp(coord->node));
    
    if (reiser4_tree_shift(tree, coord, node, SF_RIGHT)) {
	aal_exception_error("Tree failed to shift into a newly "
	    "allocated node.");
	goto error_node_free;
    }
	    
    aal_assert("vpf-640", reiser4_node_items(node) != 0, return -1);

    if (reiser4_tree_attach(tree, node)) {
	aal_exception_error("Tree failed to attach a newly allocated "
	    "node to the tree.");
	goto error_node_free;
    }

    return 0;

error_node_free:
    reiser4_node_close(node);
    return -1;
}

   // Check the level of the node we are inserting the internal item pointed 
   // to twig in.     
    level = reiser4_node_get_level(coord.node);
 
    // FIXME: Do not forget to put this into the reiser4_tree_insert 
    if (stop.top != LEAF_LEVEL || level != TWIG_LEVEL) {
	// We lookuped till the child. If we are on the border - no split is 
	// needed, otherwise - split the child to 2 parts and insert the new 
	// node between them. 

	aal_assert("vpf-663", level == stop.top, return -1);

	if ((coord.pos.item != 0 || coord.pos.unit != 0) && 
	    (coord.pos.item != reiser4_node_items(coord.node))) 
	{	    
	    // Insertable but split should be performed. 
	    if (repair_tree_shift(tree, &coord))
		return -1;

	    // Parent got changed after splitting, lookup the wanted key again 
	    // to have coord updated. 
	    stop.top = stop.bottom = level + 1;
    
	    if (reiser4_tree_lookup(tree, &hint.key, &stop, &coord)) {
		aal_exception_error("Lookup failed to find the proper place "
		    "for item insertion.");
		return -1;
	    }

	    aal_assert("vpf-664", 
		reiser4_node_get_level(coord.node) == stop.top, return -1);
	} else {
	    // Correct coord to point to parent. 
	    if (reiser4_node_pos(coord.node, &coord.pos)) 
		return -1;

	    coord.node = coord.node->parent;

	    aal_assert("vpf-662", coord.node != NULL, return -1);
	}
    } else {
	// Nothing to do - there is no one leaf yet, we found the position in 
	// the parent and node is insertable. 
    }

*/


