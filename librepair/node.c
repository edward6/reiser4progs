/*
    librepair/node.c - methods are needed for node recovery.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_node_child_max_real_key(reiser4_coord_t *parent, reiser4_key_t *key)
{
    reiser4_coord_t coord;
    errno_t res;

    aal_assert("vpf-614", parent != NULL, return -1);
    aal_assert("vpf-615", key != NULL, return -1);
    aal_assert("vpf-616", parent->item.plugin != NULL, return -1);

    if (reiser4_item_nodeptr(parent)) {
	item_entity_t *item = &parent->item;
	reiser4_ptr_hint_t ptr;

	if (plugin_call(return -1, item->plugin->item_ops, fetch, item, 
	    &ptr, parent->pos.unit, 1) != 1 || ptr.ptr == INVAL_BLK)
	    return -1;

	if (!(coord.node = reiser4_node_open(parent->node->device, ptr.ptr))) 
	    return -1;

	coord.pos.item = reiser4_node_items(coord.node) - 1;
	coord.pos.unit = ~0ul;
	
	if (reiser4_coord_realize(&coord)) {
	    aal_exception_error("Node (%llu): Failed to open the item (%u).",
		coord.node->blk, coord.pos.item);
	    goto error_child_close;
	}
	
	res = repair_node_child_max_real_key(&coord, key);
	
	if (reiser4_node_release(coord.node))
	    return -1;
    } else 
	res = reiser4_item_max_real_key(parent, key);

    return res;
    
error_child_close:
    reiser4_node_release(coord.node);
    return -1;
}

reiser4_node_t *repair_node_open(reiser4_format_t *format, blk_t blk) {
    reiser4_node_t *node;

    aal_assert("vpf-433", format != NULL, return NULL);
//    aal_assert("vpf-563", format->device != NULL, return NULL);

    /* FIXME-UMKA->VITALY */
    if ((node = reiser4_node_open(NULL/* Device should be here */, blk)) == NULL)
	return NULL;

    if (reiser4_format_get_make_stamp(format) != reiser4_node_get_make_stamp(node))
	goto error_node_free;

    return node;
    
error_node_free:
    reiser4_node_release(node);
    return NULL;
}

static errno_t repair_node_items_check(reiser4_node_t *node, 
    aux_bitmap_t *bm_used) 
{
    reiser4_coord_t coord;
    reiser4_pos_t *pos = &coord.pos;
    int res;

    aal_assert("vpf-229", node != NULL, return -1);
    aal_assert("vpf-230", node->entity != NULL, return -1);
    aal_assert("vpf-231", node->entity->plugin != NULL, return -1);
    aal_assert("vpf-529", bm_used != NULL, return -1);

    coord.node = node;
    
    for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
	pos->unit = ~0ul;
	
	/* Open the item, checking its plugin id. */
	if ((res = reiser4_coord_realize(&coord))) {
	    if (res > 0) {
		aal_exception_error("Node (%llu): Failed to open the item (%u)."
		    " Removed.", node->blk, pos->item);
	    
		if (reiser4_node_remove(node, pos)) {
		    aal_exception_bug("Node (%llu): Failed to delete the item "
			"(%d).", node->blk, pos->item);
		    return -1;
		}		
		pos->item--;
		
		continue;
	    } 

	    aal_exception_fatal("Node (%llu): Failed to open the item (%u).", 
		node->blk, pos->item);

	    return res;
	}

	/* Check that the item is legal for this node. If not, it will be 
	 * deleted in update traverse callback method. */
	if ((res = plugin_call(return -1, node->entity->plugin->node_ops, 
	    item_legal, node->entity, coord.item.plugin)))
	    return res;

	/* Check the item structure. */
	if (coord.item.plugin->item_ops.check) {
	    if ((res = coord.item.plugin->item_ops.check(&coord.item)))
		return res;
	}

	if (!reiser4_item_extent(&coord) && !reiser4_item_nodeptr(&coord))
	    continue;

	for (pos->unit = 0; pos->unit < reiser4_item_units(&coord); pos->unit++) {
	    /* FIXME-VITALY: Improve it later - it could be just width to be
	     * obviously wrong. Or start block. Give a hint into 
	     * repair_item_ptr_unused which returns what is obviously 
	     * wrong. */
	    res = repair_item_ptr_unused(&coord, bm_used);

	    if (res < 0)  
		return res;
	    else if ((res > 0) && repair_item_handle_ptr(&coord)) 
		return -1;
	}
    }
 
    return 0;    
}

static errno_t repair_node_ld_key_fetch(reiser4_node_t *node, 
    reiser4_key_t *ld_key) 
{
    reiser4_coord_t coord;
    errno_t res;
    
    aal_assert("vpf-501", node != NULL, return -1);
    aal_assert("vpf-344", ld_key != NULL, return -1);
    aal_assert("vpf-407", ld_key->plugin != NULL, return -1);

    if (node->parent != NULL) {
        if ((res = reiser4_coord_open(&coord, node->parent, &node->pos)))
	    return res;
	
	if (reiser4_item_get_key(&coord, ld_key))
	    return -1;
    } else
	reiser4_key_minimal(ld_key);
    
    return 0;
}

static errno_t repair_node_ld_key_update(reiser4_node_t *node, 
    reiser4_key_t *ld_key) 
{
    reiser4_coord_t coord;
    errno_t res;
    
    aal_assert("vpf-467", node != NULL, return -1);
    aal_assert("vpf-468", ld_key != NULL, return -1);
    aal_assert("vpf-469", ld_key->plugin != NULL, return -1);

    if (node->parent == NULL)
	return 0;

    if ((res = reiser4_coord_open(&coord, node->parent, &node->pos)))
	return res;

    return reiser4_item_set_key(&coord, ld_key);
}

errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key) {
    reiser4_coord_t coord;
    errno_t res;
    
    aal_assert("vpf-502", node != NULL, return -1);
    aal_assert("vpf-347", rd_key != NULL, return -1);
    aal_assert("vpf-408", rd_key->plugin != NULL, return -1);

    if (node->parent != NULL) {
	/* Take the right delimiting key from the parent. */
	
	if (reiser4_node_pos(coord.node, NULL))
	    return -1;
	
	/* Open coord in the parent at the correct position. */
        if ((res = reiser4_coord_open(&coord, node->parent, &node->pos)))
	    return res;
	
	/* If this is the last position in the parent, call the method 
	 * recursevely for the parent. Get the right delimiting key 
	 * otherwise. */
	
	if ((reiser4_node_items(node->parent) == coord.pos.item + 1) && 
	    (reiser4_item_units(&coord) == coord.pos.unit + 1)) 
	{
	    if (repair_node_rd_key(node->parent, rd_key))
		return -1;
	} else {
	    coord.pos.item++;
	    coord.pos.unit = 0;
	    
	    if (reiser4_coord_realize(&coord))
		return -1;

	    if (reiser4_item_get_key(&coord, rd_key))
		return -1;
	}
    } else
	reiser4_key_maximal(rd_key);
    
    return 0;
}

/* 
    FIXME-VITALY: Should this stuff be moved to plugin and how will 3.6 format be 
    supported? 
*/
errno_t repair_node_dkeys_check(reiser4_node_t *node, repair_data_t *data) {
    reiser4_coord_t coord;
    reiser4_key_t key, d_key;
    reiser4_pos_t *pos = &coord.pos;
    int res;

    aal_assert("vpf-248", node != NULL, return -1);
    aal_assert("vpf-249", node->entity != NULL, return -1);
    aal_assert("vpf-250", node->entity->plugin != NULL, return -1);
    aal_assert("vpf-240", data != NULL, return -1);

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

    coord.pos.item = 0; 
    coord.pos.unit = ~0ul;
    coord.node = node;

    if (reiser4_coord_realize(&coord))
	return -1;

    if (reiser4_item_get_key(&coord, NULL)) {
	aal_exception_error("Node (%llu): Failed to get the left key.",
	    node->blk);
	return -1;
    }

    res = reiser4_key_compare(&d_key, &coord.item.key);
    
    /* Left delimiting key should match the left key in the node. */
    if (res > 0) {
	/* The left delimiting key is much then the left key in the node - 
	 * not legal */
	aal_exception_error("Node (%llu): The first key %k is not equal to "
	    "the left delimiting key %k.", node->blk, 
	    &coord.item.key, &d_key);
	return 1;
    } else if (res < 0) {
   	/* It is legal to have the left key in the node much then its left 
	 * delimiting key - due to removing some items from the node, for 
	 * example. Fix the delemiting key if we have parent. */
	if (node->parent != NULL) {
	    aal_exception_error("Node (%llu): The left delimiting key %k in "
		"the node (%llu), pos (%u/%u) mismatch the first key %k in the "
		"node. Left delimiting key is fixed.", 
		node->blk, &coord.item.key, node->parent->blk, coord.pos.item, 
		coord.pos.unit, &d_key);
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
 
    if (reiser4_coord_realize(&coord)) {
	aal_exception_error("Node (%llu): Failed to open the item (%llu).",
	    node->blk, pos->item);
	return -1;
    }
    
    if (reiser4_item_max_real_key(&coord, &key)) {
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

static errno_t repair_node_keys_check(reiser4_node_t *node) {
    reiser4_coord_t coord;
    reiser4_key_t key, prev_key;
    reiser4_pos_t *pos = &coord.pos;
    errno_t res;
    
    aal_assert("vpf-258", node != NULL, return -1);
    
    coord.node = node;
    
    for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
	if (reiser4_coord_realize(&coord))
	    return -1;
	
	if (reiser4_item_get_key(&coord, &key)) {
	    aal_exception_error("Node (%llu): Failed to get the key of the "
		"item (%u).", node->blk, pos->item);
	    return -1;
	}
	
	if (reiser4_key_valid(&key)) {
	    aal_exception_error("Node (%llu): The key %k of the item (%u) is "
		"not valid. Item removed.", node->blk, &key, pos->item);
	    
	    if (reiser4_node_remove(node, pos)) {
		aal_exception_bug("Node (%llu): Failed to delete the item "
		    "(%d).", node->blk, pos->item);
		return -1;
	    }
	    pos->item--;
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
		return 1;		
	    }
	}
	prev_key = key;
    }
    
    return 0;
}

/*  
    Checks the node content. 
    Returns: 0 - OK; -1 - unexpected error; 1 - unrecoverable error;

    Supposed to be run with repair_check.pass.filter structure initialized.
*/
errno_t repair_node_check(reiser4_node_t *node, aux_bitmap_t *bm_used) {
    int res;
    
    aal_assert("vpf-494", node != NULL, return -1);
    aal_assert("vpf-193", node->entity != NULL, return -1);    
    aal_assert("vpf-220", node->entity->plugin != NULL, return -1);

    if ((res = plugin_call(return -1, node->entity->plugin->node_ops, 
	check, node->entity)))
	return res;

    if ((res = repair_node_items_check(node, bm_used)))
	return res;
    
    if ((res = repair_node_keys_check(node)))
	return res;
 
    if (reiser4_node_items(node) == 0)
	return 1;

    return 0;
}

errno_t repair_node_traverse(reiser4_node_t *node, rpid_t object_hint, 
    traverse_item_func_t func, void *data) 
{
    reiser4_coord_t coord;
    reiser4_pos_t *pos = &coord.pos;
    uint32_t items;

    pos->unit = ~0ul;
    for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
	if (reiser4_coord_open(&coord, node, pos)) {
	    aal_exception_error("Node (%llu), item (%u): failed to open the "
		"item by its coord.", node->blk, pos->item);
	    return -1;
	}

	if (!(object_hint & (1 << reiser4_item_type(&coord))))
	    continue;
	
	if (func(&coord, data))
		return -1;
    }

    return 0;
}


