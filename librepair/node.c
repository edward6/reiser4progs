/*
    librepair/node.c - methods are needed for node recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Callback for item_ops.layout_check which mark all correct blocks of the item 
 * layout as the given bitmap. */
static errno_t callback_item_region_check(item_entity_t *item, blk_t start, 
    uint64_t count, void *data) 
{
    aux_bitmap_t *bitmap = data;
    
    aal_assert("vpf-722", item != NULL);
    aal_assert("vpf-723", bitmap != NULL);
    aal_assert("vpf-726", start < bitmap->total && count <= bitmap->total && 
	start <= bitmap->total - count);
    
    if (!aux_bitmap_test_region_cleared(bitmap, start, count)) {
	aal_exception_error("Node (%llu), item (%u), unit (%u): points to some "
	    "already used block within (%llu - %llu).", item->con.blk, 
	    item->pos.item, item->pos.unit, start, start + count - 1);
	return 1;
    }
 
    return 0;
}

/* Sets the @key to the most right real key kept in the node or its children. */
static errno_t repair_node_child_max_real_key(reiser4_place_t *parent, reiser4_key_t *key)
{
    reiser4_place_t coord;
    errno_t res;

    aal_assert("vpf-614", parent != NULL);
    aal_assert("vpf-615", key != NULL);
    aal_assert("vpf-616", parent->item.plugin != NULL);

    if (reiser4_item_nodeptr(parent)) {
	item_entity_t *item = &parent->item;
	reiser4_ptr_hint_t ptr;

	if (plugin_call(item->plugin->item_ops, read, item, 
	    &ptr, parent->pos.unit, 1) != 1 || ptr.ptr == INVAL_BLK)
	    return -1;

	if (!(coord.node = reiser4_node_open(parent->node->device, ptr.ptr))) 
	    return -1;

	coord.pos.item = reiser4_node_items(coord.node) - 1;
	coord.pos.unit = ~0ul;
	
	if (reiser4_place_realize(&coord)) {
	    aal_exception_error("Node (%llu): Failed to open the item (%u).",
		coord.node->blk, coord.pos.item);
	    goto error_child_close;
	}
	
	res = repair_node_child_max_real_key(&coord, key);
	
	if (reiser4_node_release(coord.node))
	    return -1;
    } else 
	res = reiser4_item_utmost_key(parent, key);

    return res;
    
error_child_close:
    reiser4_node_release(coord.node);
    return -1;
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
    reiser4_node_release(node);
    return NULL;
}

/* Checks all the items of the node. */
static errno_t repair_node_items_check(reiser4_node_t *node, 
    aux_bitmap_t *bm_used) 
{
    reiser4_place_t coord;
    rpos_t *pos = &coord.pos;
    uint32_t count;
    int32_t len;
    int res;

    aal_assert("vpf-229", node != NULL);
    aal_assert("vpf-230", node->entity != NULL);
    aal_assert("vpf-231", node->entity->plugin != NULL);
    aal_assert("vpf-529", bm_used != NULL);

    coord.node = node;
    count = reiser4_node_items(node);
    
    for (pos->item = 0; pos->item < count; pos->item++) {
	pos->unit = ~0ul;
	
	/* Open the item, checking its plugin id. */
	if ((res = reiser4_place_realize(&coord))) {
	    if (res > 0) {
		aal_exception_error("Node (%llu): Failed to open the item (%u)."
		    " Removed.", node->blk, pos->item);
	    
		if (reiser4_node_remove(node, pos, 1)) {
		    aal_exception_bug("Node (%llu): Failed to delete the item "
			"(%d).", node->blk, pos->item);
		    return -1;
		}		
		pos->item--;
		count = reiser4_node_items(node);
		    
		continue;
	    } 

	    aal_exception_fatal("Node (%llu): Failed to open the item (%u).", 
		node->blk, pos->item);

	    return res;
	}

	/* Check that the item is legal for this node. If not, it will be 
	 * deleted in update traverse callback method. */
	if (!reiser4_tree_legal_level(reiser4_node_get_level(node), 
	    coord.item.plugin->h.group))
	    return 1;

	/* Check the item structure. */
	if (coord.item.plugin->item_ops.check) {
	    if ((res = coord.item.plugin->item_ops.check(&coord.item)))
		return res;
	}

	if (!reiser4_item_extent(&coord) && !reiser4_item_nodeptr(&coord))
	    continue;

	if (coord.item.plugin->item_ops.layout_check) {
	    len = plugin_call(coord.item.plugin->item_ops, 
		layout_check, &coord.item, callback_item_region_check, bm_used);

	    if (len > 0) {
		/* shrink the node */
		if ((res = plugin_call(node->entity->plugin->node_ops, 
		    shrink, node->entity, pos, len, 1)))
		{
		    aal_exception_bug("Node (%llu), item (%llu), len (%u): Failed "
			"to shrink the node on (%u) bytes.", node->blk, pos->item,
			coord.item.len, len);
		    return -1;
		}
	    } else
		return len;
	}
    }
 
    return 0;    
}

/* Sets @key to the left delimiting key of the node kept in the parent. */
static errno_t repair_node_ld_key_fetch(reiser4_node_t *node, 
    reiser4_key_t *ld_key) 
{
    reiser4_place_t coord;
    errno_t res;

    aal_assert("vpf-501", node != NULL);
    aal_assert("vpf-344", ld_key != NULL);
    aal_assert("vpf-407", ld_key->plugin != NULL);

    if (node->parent != NULL) {
        if ((res = reiser4_place_open(&coord, node->parent, &node->pos)))
	    return res;
	
	if (reiser4_item_get_key(&coord, ld_key))
	    return -1;
    } else
	reiser4_key_minimal(ld_key);
    
    return 0;
}

/* Updates the left delimiting key of the node kept in the parent. */
static errno_t repair_node_ld_key_update(reiser4_node_t *node, 
    reiser4_key_t *ld_key) 
{
    reiser4_place_t coord;
    errno_t res;
    
    aal_assert("vpf-467", node != NULL);
    aal_assert("vpf-468", ld_key != NULL);
    aal_assert("vpf-469", ld_key->plugin != NULL);

    if (node->parent == NULL)
	return 0;

    if ((res = reiser4_place_open(&coord, node->parent, &node->pos)))
	return res;

    return reiser4_item_set_key(&coord, ld_key);
}

/* Sets to the @key the right delimiting key of the node kept in the parent. */
errno_t repair_node_rd_key(reiser4_node_t *node, reiser4_key_t *rd_key) {
    reiser4_place_t coord;
    errno_t res;
    
    aal_assert("vpf-502", node != NULL);
    aal_assert("vpf-347", rd_key != NULL);
    aal_assert("vpf-408", rd_key->plugin != NULL);

    if (node->parent != NULL) {
	/* Take the right delimiting key from the parent. */
	
	if (reiser4_node_pos(node, NULL))
	    return -1;
	
	/* Open coord in the parent at the correct position. */
        if ((res = reiser4_place_open(&coord, node->parent, &node->pos)))
	    return res;
	
	/* If this is the last position in the parent, call the method 
	 * recursevely for the parent. Get the right delimiting key 
	 * otherwise. */
	
	if ((reiser4_node_items(node->parent) == coord.pos.item + 1) && 
	    (reiser4_item_units(&coord) == coord.pos.unit + 1 || 
	     coord.pos.unit == ~0ul)) 
	{
	    if (repair_node_rd_key(node->parent, rd_key))
		return -1;
	} else {
	    coord.pos.item++;
	    coord.pos.unit = 0;
	    
	    if (reiser4_place_realize(&coord))
		return -1;

	    if (reiser4_item_get_key(&coord, rd_key))
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
errno_t repair_node_dkeys_check(reiser4_node_t *node, repair_data_t *data) {
    reiser4_place_t coord;
    reiser4_key_t key, d_key;
    rpos_t *pos = &coord.pos;
    int res;

    aal_assert("vpf-248", node != NULL);
    aal_assert("vpf-249", node->entity != NULL);
    aal_assert("vpf-250", node->entity->plugin != NULL);
    aal_assert("vpf-240", data != NULL);

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

    if (reiser4_place_realize(&coord))
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
 
    if (reiser4_place_realize(&coord)) {
	aal_exception_error("Node (%llu): Failed to open the item (%llu).",
	    node->blk, pos->item);
	return -1;
    }
    
    if (reiser4_item_utmost_key(&coord, &key)) {
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
static errno_t repair_node_keys_check(reiser4_node_t *node) {
    reiser4_place_t coord;
    reiser4_key_t key, prev_key;
    rpos_t *pos = &coord.pos;
    uint32_t count;
    errno_t res;
    
    aal_assert("vpf-258", node != NULL);
    
    coord.node = node;
    count = reiser4_node_items(node);
    
    for (pos->item = 0; pos->item < count; pos->item++) {
	if (reiser4_place_realize(&coord))
	    return -1;
	
	if (reiser4_item_get_key(&coord, &key)) {
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
    
    aal_assert("vpf-494", node != NULL);
    aal_assert("vpf-193", node->entity != NULL);    
    aal_assert("vpf-220", node->entity->plugin != NULL);

    if ((res = plugin_call(node->entity->plugin->node_ops, 
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

/* Traverse through all items of the gived node. */
errno_t repair_node_traverse(reiser4_node_t *node, traverse_item_func_t func, 
    void *data) 
{
    reiser4_place_t coord;
    rpos_t *pos = &coord.pos;
    uint32_t items;

    aal_assert("vpf-744", node != NULL);
    
    pos->unit = ~0ul;

    for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
	if (reiser4_place_open(&coord, node, pos)) {
	    aal_exception_error("Node (%llu), item (%u): failed to open the "
		"item by its coord.", node->blk, pos->item);
	    return -1;
	}

	if (func(&coord, data))
		return -1;
    }

    return 0;
}


