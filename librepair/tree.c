/*
    tree.c -- repair/tree.c -- tree auxiliary code.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/*
  This function returns TRUE if passed item @group corresponds to passed @level
  Hardcoded method, valid for the current tree imprementation only.
*/
bool_t repair_tree_legal_level(reiser4_item_group_t group,
			       uint8_t level)
{
    if (group == NODEPTR_ITEM) {
	if (level == LEAF_LEVEL)
	    return FALSE;
    } else if (group == EXTENT_ITEM) {
	if (level != TWIG_LEVEL)
	    return FALSE;
    } else
	return level == LEAF_LEVEL;

    return TRUE;
}

static errno_t callback_data_level(reiser4_plugin_t *plugin,
    void *data)
{
    uint8_t *level = (uint8_t *)data;

    aal_assert("vpf-746", data != NULL);

    if (!repair_tree_legal_level(plugin->h.group, *level))
	return 0;

    return reiser4_item_data(plugin);
}

bool_t repair_tree_data_level(uint8_t level) {

    if (level == 0)
	return FALSE;
	
    return (libreiser4_factory_cfind(callback_data_level,
	&level) != NULL);
}

/* Get the max real key existed in the tree. Go down through all right-most 
 * child to get it. */
static errno_t repair_tree_max_real_key(reiser4_node_t *node, 
    reiser4_key_t *key) 
{
    reiser4_place_t place;
    reiser4_node_t *child;
    errno_t res;

    aal_assert("vpf-712", node != NULL);
    aal_assert("vpf-713", key != NULL);

    place.node = node;
    place.pos.item = reiser4_node_items(node) - 1;
    place.pos.unit = ~0ul;

    if (reiser4_place_realize(&place)) {
	aal_exception_error("Node (%llu): Failed to open the item (%u).",
	    node->blk, place.pos.item);
	return -1;
    }
 
    if (reiser4_item_nodeptr(&place)) {
	item_entity_t *item = &place.item;
	reiser4_ptr_hint_t ptr;

	place.pos.unit = reiser4_item_units(&place);
	
	if (plugin_call(item->plugin->item_ops, read, item, 
	    &ptr, place.pos.unit, 1) != 1 || ptr.ptr == INVAL_BLK)
	    return -1;

	if (!(child = reiser4_node_open(place.node->device, ptr.ptr))) 
	    return -1;
	
	res = repair_tree_max_real_key(child, key);
	
	if (reiser4_node_close(child))
	    return -1;
    } else 
	res = reiser4_item_utmost_key(&place, key);

    return res;
}

/* Corrects place for insertion over the base reiser4_tree_lookup method. */
lookup_t repair_tree_lookup(reiser4_tree_t *tree, reiser4_key_t *key, 
    reiser4_place_t *place) 
{
    uint32_t items;
    lookup_t lookup;
     
    if ((lookup = reiser4_tree_lookup(tree, key, LEAF_LEVEL, place)) == LP_FAILED) {
	aal_stream_t stream;
	
	aal_stream_init(&stream);
	reiser4_key_print(key, &stream);
	aal_exception_error("Lookup of key %s failed.", stream.data);			
	aal_stream_fini(&stream);

	return lookup;
    } else if (lookup == LP_PRESENT) {
	if (reiser4_place_realize(place))
	    return LP_FAILED;
	
	return lookup;
    }

    items = reiser4_node_items(place->node);

    /* Position was not found - place could point to a non existent position, 
     * move to the right item then. */
    if (place->pos.item < items) {
	if (reiser4_place_realize(place))
	    return LP_FAILED;

	if (place->pos.unit == reiser4_item_units(place)) {
	    place->pos.item++; 
	    place->pos.unit = ~0ul;

	    if (place->pos.item < items) {
		if (reiser4_place_realize(place))
		    return LP_FAILED;
	    }
	} 
    }   

    return LP_ABSENT;
}

/* This function creates nodeptr item on the nase of 'node' and insert it to 
 * the tree. */
errno_t repair_tree_attach(reiser4_tree_t *tree, reiser4_node_t *node) {
    reiser4_place_t place;
    reiser4_item_hint_t hint;
    reiser4_ptr_hint_t ptr;
    reiser4_key_t rkey, key;
    errno_t res;
    rpid_t pid;
    uint32_t level;
    lookup_t lookup;

    aal_assert("vpf-658", tree != NULL);
    aal_assert("vpf-659", node != NULL);

    /* Stop at the same level to be able to split the found node and insert 
     * the passed node between its parts. */
    level = reiser4_node_get_level(node);

    while (level >= reiser4_tree_height(tree))
	reiser4_tree_growup(tree);

    /* Preparing nodeptr item hint */
    aal_memset(&hint, 0, sizeof(hint));
    aal_memset(&ptr, 0, sizeof(ptr));

    reiser4_node_lkey(node, &hint.key);

    if ((lookup = repair_tree_lookup(tree, &hint.key, &place)) != LP_ABSENT)
	return lookup;
	
    /* Key does not exist in the tree. Check the found position. Try to split 
     * the node to insert the whole node. */

    if (place.pos.item == reiser4_node_items(place.node)) {
	if (repair_node_rd_key(place.node, &key))	    
	    return -1;
    } else {
	if (reiser4_item_get_key(&place, &key)) 
	    return -1;
    }
    
    if (repair_tree_max_real_key(node, &rkey))
	return -1;
 
    if (reiser4_key_compare(&key, &rkey) >= 0)
	return 1;
 
    hint.type_specific = &ptr;
    hint.count = 1;
    hint.flags = HF_FORMATD;
    ptr.ptr = node->blk;
    ptr.width = 1;

    pid = reiser4_profile_value(tree->fs->profile, "nodeptr");

    if (!(hint.plugin = libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) {
	aal_exception_error("Can't find item plugin by its id 0x%x.", pid);
	return -1;
    }

    /* Split the found place if needed to insert the whole node. */
    if (reiser4_tree_split(tree, &place, level))
	return -1;
    
    if ((res = reiser4_tree_insert(tree, &place, level, &hint))) {
	aal_exception_error("Can't insert nodeptr item to the tree.");
	return res;
    }

    /* Setting needed links between nodes in the tree cashe. */
    if (reiser4_tree_connect(tree, place.node, node)) {
	aal_exception_error("Can't attach the node %llu in tree cache.", 
	    node->blk);
	return -1;
    }

    return 0;
}

/* Insert the item with overwriting of existent in the tree items if needed. */
errno_t repair_tree_insert(reiser4_tree_t *tree, reiser4_place_t *insert) {
    reiser4_place_t place;
    reiser4_key_t src_key, dst_key;
    lookup_t res;
    uint32_t count;

    aal_assert("vpf-654", tree != NULL);
    aal_assert("vpf-655", insert != NULL);
    aal_assert("vpf-657", insert->node != NULL);
    aal_assert("vpf-656", insert->node->tree != NULL);

    insert->pos.unit = 0;
    while (insert->pos.unit < reiser4_item_units(insert)) {
	if (reiser4_item_get_key(insert, NULL)) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): failed to get "
		"the item key.", insert->node->blk, insert->pos.item,
		insert->pos.unit);
	    return -1;
	}
	
	res = repair_tree_lookup(tree, &insert->item.key, &place);

	if (res == LP_ABSENT) {
	    /* Start key does not exist in the tree. Prepare the insertion. */

	    if (place.pos.item == reiser4_node_items(place.node)) {
		if (repair_node_rd_key(place.node, &dst_key))
		    return -1;
	    } else {
		if (reiser4_item_get_key(&place, &dst_key)) 
		    return -1;
	    }

	    /* Count of items to be inserted. */
	    if ((count = repair_item_split(insert, &dst_key)) == (uint32_t)-1)
		return -1;

	    aal_assert("vpf-681", count > insert->pos.unit);

	    count -= insert->pos.unit;
	} else if (res == LP_PRESENT) {
	    /* Prepare the overwriting. */

	    /* There are some item plugins which have gaps in keys between their 
	     * units - like direntry40 - check that. Use the special method - 
	     * item_ops.gap_key - which get the max real key stored continously 
	     * from the key specified in the place. */

	    if (place.item.plugin->h.id != insert->item.plugin->h.id) {
		/* FIXME: relocation code should be here. */
		aal_exception_error("Tree failed to overwrite items of "
		    "different plugins. Relocation is not supported yet.");
		return -1;
	    }

	    if (reiser4_item_gap_key(&place, &dst_key)) 
		return -1;

	    if (reiser4_item_utmost_key(insert, &src_key)) 
		return -1;
	    
	    /* Count of items to be inserted. */
	    if (reiser4_key_get_offset(&dst_key) >= reiser4_key_get_offset(&src_key)) {
		count = reiser4_item_units(insert) - insert->pos.unit;
	    } else {
		if ((count = repair_item_split(insert, &dst_key)) == (uint32_t)-1)
		    return -1;

		aal_assert("vpf-682", count > insert->pos.unit);

		count -= insert->pos.unit;
	    }
	} else {
	    return res;
	}

	/* FIXME: Here tree_write_prepare should be called. It should split the 
	 * target node to keep correct flush_ids. */
	
	if (reiser4_tree_write(tree, &place, insert, count)) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u), count "
		"(%u): Tree failed on writing the set of units to the tree "
		"position: node (%llu), item (%u), unit (%u).",
		insert->node->blk, insert->pos.item, insert->pos.unit, count,
		place.node->blk, place.pos.item, place.pos.unit);
	    return -1;
	}

	insert->pos.unit += count;
    }

    return 0;
}

/* 
 * Check the place for key range insertion possibility. 
 * Split found node if it helps. 
 * 
 * FIXME: This method implies that it is run after lookup and the wanted key 
 * was not found. So start_key is not checked here.
 */
/*
static errno_t repair_tree_fits(reiser4_place_t *place,
    reiser4_key_t *start_key, reiser4_key_t *end_key)
{
    reiser4_key_t key;
    uint32_t items, units;    

    aal_assert("vpf-596", place != NULL);
    aal_assert("vpf-660", place->node != NULL);
    
    if (place->pos.item == items) {
	if (repair_node_rd_key(place->node, &key))
	    return -1;
    } else {
	if (reiser4_place_realize(place)) {
	    aal_exception_error("Node (%llu): failed to open the item on "
		"the parent node.", place->node->blk);
	    return -1;
	}

	aal_assert("vpf-671", place->pos.unit < reiser4_item_units(place));
	
	if (reiser4_item_get_key(place, &key)) {
	    aal_exception_error("Node (%llu): failed to get the item key "
		"by its place.", place->node->blk);	    
	    return -1;
	}
    }

    if (reiser4_key_compare(&key, end_key) >= 0)
	return 1;
    
    return 0;
}
*/

/*
 
static errno_t repair_tree_shift(reiser4_tree_t *tree, reiser4_place_t *place) {
    reiser4_node_t *node;
    uint32_t level;
    
    aal_assert("vpf-665", place != NULL);
    aal_assert("vpf-666", place->node != NULL);
    aal_assert("vpf-667", place->node->tree != NULL);

    if ((place->pos.item == 0 && place->pos.unit == 0) || 
	(place->pos.item == reiser4_node_items(place->node)))
	return 0;

    // Insertable but split should be performed. 
    // FIXME: place could be realised already. Optimise it later. 
    if (reiser4_place_realize(place))
	return -1;
		
    level = reiser4_node_level(place->node);
    
    if ((node = reiser4_tree_allocate(tree, level)) == NULL) {
	aal_exception_error("Tree failed to allocate a new node.");
	return -1;
    }

    // set flush_id 
    reiser4_node_set_flush_stamp(node, 
	reiser4_node_get_flush_stamp(place->node));
    
    if (reiser4_tree_shift(tree, place, node, SF_RIGHT)) {
	aal_exception_error("Tree failed to shift into a newly "
	    "allocated node.");
	goto error_node_free;
    }
	    
    aal_assert("vpf-640", reiser4_node_items(node) != 0);

    if (reiser4_tree_attach(tree, node)) {
	aal_exception_error("Tree failed to attach a newly allocated "
	    "node to the tree.");
	goto error_node_free;
    }

    return 0;

error_node_free:
    reiser4_node_release(node);
    return -1;
}

   // Check the level of the node we are inserting the internal item pointed 
   // to twig in.     
    level = reiser4_node_get_level(place.node);
 
    // FIXME: Do not forget to put this into the reiser4_tree_insert 
    if (stop.top != LEAF_LEVEL || level != TWIG_LEVEL) {
	// We lookuped till the child. If we are on the border - no split is 
	// needed, otherwise - split the child to 2 parts and insert the new 
	// node between them. 

	aal_assert("vpf-663", level == stop.top);

	if ((place.pos.item != 0 || place.pos.unit != 0) && 
	    (place.pos.item != reiser4_node_items(place.node))) 
	{	    
	    // Insertable but split should be performed. 
	    if (repair_tree_shift(tree, &place))
		return -1;

	    // Parent got changed after splitting, lookup the wanted key again 
	    // to have place updated. 
	    stop.top = stop.bottom = level + 1;
    
	    if (reiser4_tree_lookup(tree, &hint.key, &stop, &place)) {
		aal_exception_error("Lookup failed to find the proper place "
		    "for item insertion.");
		return -1;
	    }

	    aal_assert("vpf-664", 
		reiser4_node_get_level(place.node) == stop.top);
	} else {
	    // Correct place to point to parent. 
	    if (reiser4_node_pos(place.node, &place.pos)) 
		return -1;

	    place.node = place.node->parent;

	    aal_assert("vpf-662", place.node != NULL);
	}
    } else {
	// Nothing to do - there is no one leaf yet, we found the position in 
	// the parent and node is insertable. 
    }

*/


