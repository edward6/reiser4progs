/*
    librepair/node.c - methods are needed for node recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Get the max real key existed in the tree. Go down through all right-most 
 * child to get it. */
errno_t repair_node_max_real_key(reiser4_node_t *node, reiser4_key_t *key) {
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
 
    if (reiser4_item_branch(&place)) {
	item_entity_t *item = &place.item;
	reiser4_ptr_hint_t ptr;

	place.pos.unit = reiser4_item_units(&place) - 1;
	
	if (plugin_call(item->plugin->item_ops, read, item, 
	    &ptr, place.pos.unit, 1) != 1 || ptr.ptr == INVAL_BLK)
	    return -1;

	if (!(child = reiser4_node_open(place.node->device, ptr.ptr))) 
	    return -1;
	
	res = repair_node_max_real_key(child, key);
	
	if (reiser4_node_close(child))
	    return -1;
    } else 
	res = reiser4_item_utmost_key(&place, key);

    return res;
}

/* Opens the node if it has correct mkid stamp. */
reiser4_node_t *repair_node_open(reiser4_fs_t *fs, blk_t blk) {
    reiser4_node_t *node;

    aal_assert("vpf-708", fs != NULL);

    if ((node = reiser4_node_open(fs->device, blk)) == NULL)
	return NULL;

    if (reiser4_format_get_stamp(fs->format) != reiser4_node_get_mstamp(node))
	goto error_node_free;

    return node;

error_node_free:
    reiser4_node_close(node);
    return NULL;
}

/* Checks all the items of the node. */
static errno_t repair_node_items_check(reiser4_node_t *node, 
    uint8_t mode) 
{
    reiser4_place_t place;
    pos_t *pos = &place.pos;
    uint32_t count;
    int32_t len;
    errno_t res = REPAIR_OK;

    aal_assert("vpf-229", node != NULL);
    aal_assert("vpf-230", node->entity != NULL);
    aal_assert("vpf-231", node->entity->plugin != NULL);

    place.node = node;
    count = reiser4_node_items(node);
    
    for (pos->item = 0; pos->item < count; pos->item++) {
	pos->unit = ~0ul;
	
	/* Open the item, checking its plugin id. */
	if (reiser4_place_realize(&place)) {
	    aal_exception_error("Node (%llu): Failed to open the item (%u)."
		" %s", node->blk, pos->item, mode == REPAIR_REBUILD ? 
		"Removed." : "");
	    
	    if (mode == REPAIR_REBUILD) {
		if (reiser4_node_remove(node, pos, 1)) {
		    aal_exception_bug("Node (%llu): Failed to delete the item "
			"(%d).", node->blk, pos->item);
		    return -1;
		}
		pos->item--;
		count = reiser4_node_items(node);
		res |= REPAIR_FIXED;
	    } else 
		res |= REPAIR_FATAL;
	    
	    continue;
	}

	/* Check that the item is legal for this node. If not, it will be 
	 * deleted in update traverse callback method. */
	if (!repair_tree_legal_level(place.item.plugin->h.group, 
	    reiser4_node_get_level(node)))
	{
	    aal_exception_error("Node (%llu): Node level (%u) does not match "
		"to the item type (%s).", node->blk, 
		reiser4_node_get_level(node), place.item.plugin->h.label);
	    /* FIXME-VITALY: smth should be done here later. */
	    res |= REPAIR_FATAL;
	}

	/* Check the item structure. */
	res |= repair_item_check(&place, mode);
	
	if (res < 0)
	    return res;
    
	if (res & REPAIR_REMOVED) {
	    pos->item--;
	    count = reiser4_node_items(node);
	    res &= ~REPAIR_REMOVED;
	    res |= REPAIR_FIXED;
	}
	
#if 0
	if (place.item.plugin->item_ops.layout_check) {
	    uint32_t lenght = place.item.len;
	    
	    len = plugin_call(place.item.plugin->item_ops, 
		layout_check, &place.item, callback_item_region_check, bm_used);

	    if (len > 0) {
		//aal_assert("vpf-790", len < lenght);
		
		/* shrink the node */
		if (len) {
		    pos->unit = 0;
		    
		    if (reiser4_node_shrink(node, pos, len, 1)) {
			aal_exception_bug("Node (%llu), item (%llu), len (%u): "
			    "Failed to shrink the node on (%u) bytes.", 
			    node->blk, pos->item, place.item.len, len);
			return -1;
		    }
		} else {
		    
		}
	    } else
		return len;
	}
#endif
	
    }

    return res;
}

/* Sets @key to the left delimiting key of the node kept in the parent. */
static errno_t repair_node_ld_key_fetch(reiser4_node_t *node, 
    reiser4_key_t *ld_key) 
{
    reiser4_place_t place;
    errno_t res;

    aal_assert("vpf-501", node != NULL);
    aal_assert("vpf-344", ld_key != NULL);
    aal_assert("vpf-407", ld_key->plugin != NULL);

    if (node->parent.node != NULL) {
        if ((res = reiser4_place_open(&place, node->parent.node,
				      &node->parent.pos)))
	    return res;
	
	if (reiser4_item_get_key(&place, ld_key))
	    return -1;
    } else
	reiser4_key_minimal(ld_key);
    
    return 0;
}

/* Updates the left delimiting key of the node kept in the parent. */
static errno_t repair_node_ld_key_update(reiser4_node_t *node, 
    reiser4_key_t *ld_key) 
{
    reiser4_place_t place;
    errno_t res;
    
    aal_assert("vpf-467", node != NULL);
    aal_assert("vpf-468", ld_key != NULL);
    aal_assert("vpf-469", ld_key->plugin != NULL);

    if (node->parent.node == NULL)
	return 0;

    if ((res = reiser4_place_open(&place, node->parent.node,
				  &node->parent.pos)))
	return res;

    return reiser4_item_set_key(&place, ld_key);
}

/* Sets to the @key the right delimiting key of the node kept in the parent. */
errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key) {
    reiser4_place_t place;
    errno_t res;
    
    aal_assert("vpf-502", node != NULL);
    aal_assert("vpf-347", rd_key != NULL);
    aal_assert("vpf-408", rd_key->plugin != NULL);

    if (node->parent.node != NULL) {
	/* Take the right delimiting key from the parent. */
	
	if (reiser4_node_pos(node, NULL))
	    return -1;
	
	/* Open place in the parent at the correct position. */
        if ((res = reiser4_place_open(&place, node->parent.node,
				      &node->parent.pos)))
	    return res;
	
	/* If this is the last position in the parent, call the method 
	 * recursevely for the parent. Get the right delimiting key 
	 * otherwise. */
	
	if ((reiser4_node_items(node->parent.node) == place.pos.item + 1) && 
	    (reiser4_item_units(&place) == place.pos.unit + 1 || 
	     place.pos.unit == ~0ul)) 
	{
	    if (repair_node_rd_key(node->parent.node, rd_key))
		return -1;
	} else {
	    place.pos.item++;
	    place.pos.unit = 0;
	    
	    if (reiser4_place_realize(&place))
		return -1;

	    if (reiser4_item_get_key(&place, rd_key))
		return -1;
	}
    } else
	reiser4_key_maximal(rd_key);
    
    return 0;
}

/* 
    FIXME-VITALY: Should this stuff be moved to plugin (tree plugin?) and how 
    will 3.6 format be supported?
*/
/* Checks the delimiting keys of the node kept in the parent. */
errno_t repair_node_dkeys_check(reiser4_node_t *node) {
    reiser4_place_t place;
    reiser4_key_t key, d_key;
    pos_t *pos = &place.pos;
    int res;

    aal_assert("vpf-248", node != NULL);
    aal_assert("vpf-249", node->entity != NULL);
    aal_assert("vpf-250", node->entity->plugin != NULL);

    /* FIXME-VITALY: Fixed plugin id is used for key. */
    if (!(d_key.plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, 
	KEY_REISER40_ID))) 
    {
	aal_exception_error("Can't find key plugin by its id 0x%x.", 
	    KEY_REISER40_ID);
	return -1;
    }

    if (repair_node_ld_key_fetch(node, &d_key)) {
	aal_exception_error("Node (%llu): Failed to get the left delimiting key.", 
	    node->blk);
	return -1;
    }

    place.pos.item = 0; 
    place.pos.unit = ~0ul;
    place.node = node;

    if (reiser4_place_realize(&place))
	return -1;

    if (reiser4_item_get_key(&place, NULL)) {
	aal_exception_error("Node (%llu): Failed to get the left key.",
	    node->blk);
	return -1;
    }

    res = reiser4_key_compare(&d_key, &place.item.key);
    
    /* Left delimiting key should match the left key in the node. */
    if (res > 0) {
	/* The left delimiting key is much then the left key in the node - 
	 * not legal */
	aal_exception_error("Node (%llu): The first key %k is not equal to "
	    "the left delimiting key %k.", node->blk, 
	    &place.item.key, &d_key);
	return 1;
    } else if (res < 0) {
   	/* It is legal to have the left key in the node much then its left 
	 * delimiting key - due to removing some items from the node, for 
	 * example. Fix the delemiting key if we have parent. */
	if (node->parent.node != NULL) {
	    aal_exception_error("Node (%llu): The left delimiting key %k in "
		"the node (%llu), pos (%u/%u) mismatch the first key %k in the "
		"node. Left delimiting key is fixed.", 
		node->blk, &place.item.key, node->parent.node->blk, place.pos.item, 
		place.pos.unit, &d_key);
	    if (repair_node_ld_key_update(node, &d_key)) 
		return -1;
	}
    }
    
    if (repair_node_rd_key(node, &d_key)) {
	aal_exception_error("Node (%llu): Failed to get the right delimiting "
	    "key.", node->blk);
	return -1;
    }

    pos->item = reiser4_node_items(node) - 1;
 
    if (reiser4_place_realize(&place)) {
	aal_exception_error("Node (%llu): Failed to open the item (%llu).",
	    node->blk, pos->item);
	return -1;
    }
    
    if (reiser4_item_utmost_key(&place, &key)) {
	aal_exception_error("Node (%llu): Failed to get the max real key of "
	    "the last item.", node->blk);
	return -1;
    }
    
    if (reiser4_key_compare(&key, &d_key) >= 0) {
	aal_exception_error("Node (%llu): The last key %k in the node is less "
	    "then the right delimiting key %k.", 
	    node->blk, &key, &d_key);
	return 1;
    }

    return 0;
}

/* Checks the set of keys of the node. */
static errno_t repair_node_keys_check(reiser4_node_t *node, uint8_t mode) {
    reiser4_place_t place;
    reiser4_key_t key, prev_key;
    pos_t *pos = &place.pos;
    uint32_t count;
    errno_t res;
    
    aal_assert("vpf-258", node != NULL);
    
    place.node = node;
    count = reiser4_node_items(node);
    
    for (pos->item = 0; pos->item < count; pos->item++) {
	if (reiser4_place_realize(&place))
	    return -1;
	
	if (reiser4_item_get_key(&place, &key)) {
	    aal_exception_error("Node (%llu): Failed to get the key of the "
		"item (%u).", node->blk, pos->item);
	    return -1;
	}
	
	if (reiser4_key_valid(&key)) {
	    aal_exception_error("Node (%llu): The key %k of the item (%u) is "
		"not valid. Item removed.", node->blk, &key, pos->item);
	    
	    if (reiser4_node_remove(node, pos, 1)) {
		aal_exception_bug("Node (%llu): Failed to delete the item "
		    "(%d).", node->blk, pos->item);
		return -1;
	    }
	    pos->item--;
	    count = reiser4_node_items(node);
	    
	    continue;
	}
	
	if (pos->item) {
	    if ((res = reiser4_key_compare(&prev_key, &key)) > 0 || 
		(res == 0 && (reiser4_key_get_type(&key) != KEY_FILENAME_TYPE ||
		reiser4_key_get_type(&prev_key) != KEY_FILENAME_TYPE))) 
	    {
		/* 
		    FIXME-VITALY: Which part does put the rule that neighbour 
		    keys could be equal?
		*/
		aal_exception_error("Node (%llu), items (%u) and (%u): Wrong "
		    "order of keys.", node->blk, pos->item - 1, pos->item);

		return REPAIR_FATAL;
	    }
	}
	prev_key = key;
    }
    
    return REPAIR_OK;
}

/*  
    Checks the node content. 
    Returns values according to repair_error_codes_t.
*/
errno_t repair_node_check(reiser4_node_t *node, uint8_t mode) {
    errno_t res = REPAIR_OK;
    
    aal_assert("vpf-494", node != NULL);
    aal_assert("vpf-193", node->entity != NULL);    
    aal_assert("vpf-220", node->entity->plugin != NULL);

    res |= plugin_call(node->entity->plugin->node_ops, check, 
	node->entity, mode);

    if (repair_error_fatal(res))
	return res;
    
    res |= repair_node_items_check(node, mode);

    if (repair_error_fatal(res))
	return res;
    
    res |= repair_node_keys_check(node, mode);
    
    if (repair_error_fatal(res))
	return res;

    return res;
}

/* Traverse through all items of the gived node. */
errno_t repair_node_traverse(reiser4_node_t *node, traverse_item_func_t func, 
    void *data) 
{
    reiser4_place_t place;
    pos_t *pos = &place.pos;
    uint32_t items;

    aal_assert("vpf-744", node != NULL);
    
    pos->unit = ~0ul;

    for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
	if (reiser4_place_open(&place, node, pos)) {
	    aal_exception_error("Node (%llu), item (%u): failed to open the "
		"item by its place.", node->blk, pos->item);
	    return -1;
	}

	if (func(&place, data))
		return -1;
    }

    return 0;
}

