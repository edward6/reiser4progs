/*
    librepair/node.c - methods are needed for node recovery.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_node_items_check(reiser4_node_t *node, 
    repair_data_t *data) 
{
    reiser4_coord_t coord;
    reiser4_pos_t pos;
    rpid_t pid;
    int res;

    aal_assert("vpf-229", node != NULL, return -1);
    aal_assert("vpf-230", node->entity != NULL, return -1);
    aal_assert("vpf-231", node->entity->plugin != NULL, return -1);
    aal_assert("vpf-242", data != NULL, return -1);

    for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++) {
	pos.unit = ~0ul;
	
	/* Open the item, checking its plugin id. */
	if ((res = repair_coord_open(&coord, node, CT_NODE, &pos))) {
	    if (res > 0) {
		aal_exception_error("Node (%llu): Failed to open the item (%u)."
		    " Removed.", node->blk, pos.item);
	    
		if (reiser4_node_remove(node, &pos)) {
		    aal_exception_bug("Node (%llu): Failed to delete the item "
			"(%d).", node->blk, pos.item);
		    return -1;
		}		
		pos.item--;
		
		continue;		
	    } 

	    aal_exception_fatal("Node (%llu): Failed to open the item (%u).", 
		node->blk, pos.item);

	    return res;
	}
	
	/* Check that the item is legal for this node. If not, it will be 
	 * deleted in update traverse callback method. */
	if ((res = plugin_call(return -1, node->entity->plugin->node_ops, 
	    item_legal, node->entity, coord.entity.plugin)))
	    return res;

	/* Check the item structure. */
	if (coord.entity.plugin->item_ops.check) {
	    if ((res = coord.entity.plugin->item_ops.check(&coord.entity)))
		return res;
	}

	if (!reiser4_item_extent(&coord) && !reiser4_item_nodeptr(&coord))
	    continue;
	
	for (pos.unit = 0; pos.unit < reiser4_item_count(&coord); pos.unit++) {
	    /* FIXME-VITALY: Improve it later - it could be just width to be
	     * obviously wrong. Or start block. Give a hint into 
	     * repair_item_ptr_used_in_format which returns what is obviously 
	     * wrong. */
	    if ((res = repair_item_ptr_used_in_bitmap(&coord, 
		data->bm_format_layout, data)) < 0)  
		return res;
	    else if ((res > 0) && repair_item_handle_ptr(&coord)) 
		return -1;
	}
    }
 
    return 0;    
}

static errno_t repair_joint_ld_key_fetch(reiser4_joint_t *joint, 
    reiser4_key_t *ld_key, repair_data_t *data) 
{
    reiser4_coord_t coord;
    errno_t res;
    
    aal_assert("vpf-501", joint != NULL, return -1);
    aal_assert("vpf-344", ld_key != NULL, return -1);
    aal_assert("vpf-407", ld_key->plugin != NULL, return -1);

    if (joint->parent != NULL) {
        if ((res = reiser4_coord_open(&coord, joint->parent, CT_JOINT, &joint->pos)))
	    return res;
	
	return reiser4_item_key(&coord, ld_key);
    }

    reiser4_key_minimal(ld_key);
    
    return 0;
}

static errno_t repair_joint_ld_key_update(reiser4_joint_t *joint, 
    reiser4_key_t *ld_key, repair_data_t *data) 
{
    reiser4_coord_t coord;
    errno_t res;
    
    aal_assert("vpf-467", joint != NULL, return -1);
    aal_assert("vpf-468", ld_key != NULL, return -1);
    aal_assert("vpf-469", ld_key->plugin != NULL, return -1);

    if (joint->parent == NULL)
	return 0;

    if ((res = reiser4_coord_open(&coord, joint->parent, CT_JOINT, &joint->pos)))
	return res;

    return reiser4_item_update(&coord, ld_key);
}

static errno_t repair_joint_rd_key(reiser4_joint_t *joint, 
    reiser4_key_t *rd_key, repair_data_t *data)
{
    reiser4_coord_t coord;
    reiser4_pos_t pos = {0, 0};
    errno_t res;
    
    aal_assert("vpf-502", joint != NULL, return -1);
    aal_assert("vpf-347", rd_key != NULL, return -1);
    aal_assert("vpf-408", rd_key->plugin != NULL, return -1);
    aal_assert("vpf-348", data != NULL, return -1);

    if (joint->parent != NULL) {
	aal_assert("vpf-413", joint->parent->node != NULL, return -1);
	
	/* Open coord in the parent at the correct position. */
        if ((res = reiser4_coord_open(&coord, joint->parent, CT_JOINT, 
	        &joint->pos)))
	    return res;
	
	/* If this is the last position in the parent, call the method 
	 * recursevely for the parent. Get the right delimiting key 
	 * otherwise. */
	if (reiser4_node_count(joint->parent->node) == coord.pos.item + 1) {
	    return repair_joint_rd_key(joint->parent, rd_key, data);
	} else {
	    pos.item = coord.pos.item + 1;
	    if (reiser4_coord_open(&coord, joint->parent, CT_JOINT, &pos))
		return -1;

	    return reiser4_item_key(&coord, rd_key);
	}
    }

    reiser4_key_maximal(rd_key);
    
    return 0;
}

/* 
    FIXME-VITALY: Should this stuff be moved to plugin and how will 3.6 format be 
    supported? 
*/
errno_t repair_joint_dkeys_check(reiser4_joint_t *joint, 
    repair_data_t *data) 
{
    reiser4_coord_t coord;
    reiser4_key_t key, d_key;
    reiser4_pos_t pos = {0, ~0ul};
    int res;

    aal_assert("vpf-248", joint != NULL, return -1);
    aal_assert("vpf-395", joint->node != NULL, return -1);
    aal_assert("vpf-249", joint->node->entity != NULL, return -1);
    aal_assert("vpf-250", joint->node->entity->plugin != NULL, return -1);
    aal_assert("vpf-240", data != NULL, return -1);
    aal_assert("vpf-241", data->format != NULL, return -1);

    /* FIXME-VITALY: Fixed plugin id is used for key. */
    if (!(d_key.plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, 
	KEY_REISER40_ID))) 
    {
	aal_exception_error("Can't find key plugin by its id 0x%x.", 
	    KEY_REISER40_ID);
	return -1;
    }

    key.plugin = d_key.plugin;

    if (repair_joint_ld_key_fetch(joint, &d_key, data)) {
	aal_exception_error("Node (%llu): Failed to get the left delimiting key.", 
	    joint->node->blk);
	return -1;
    }
    
    if (reiser4_coord_open(&coord, joint, CT_JOINT, &pos))
	return -1;

    if (reiser4_item_key(&coord, &key)) {
	aal_exception_error("Node (%llu): Failed to get the left key.",
	    joint->node->blk);
	return -1;
    }

    res = reiser4_key_compare(&d_key, &key);
    
    /* Left delimiting key should match the left key in the node. */
    if (res > 0) {
	/* The left delimiting key is much then the left key in the node - 
	 * not legal */
	aal_exception_error("Node (%llu): The first key %k is not equal to "
	    "the left delimiting key %k.", joint->node->blk, 
	    &key, &d_key);
	return 1;
    } else if (res < 0) {
   	/* It is legal to have the left key in the node much then its left 
	 * delimiting key - due to removing some items from the node, for 
	 * example. Fix the delemiting key if we have parent. */
	if (joint->parent != NULL) {
	    aal_exception_error("Node (%llu): The left delimiting key %k in "
		"the node (%llu), pos (%u/%u) mismatch the first key %k in the "
		"node. Left delimiting key is fixed.", 
		joint->node->blk, &key, joint->parent->node->blk, coord.pos.item, 
		coord.pos.unit, &d_key);
	    if (repair_joint_ld_key_update(joint, &d_key, data)) 
		return -1;
	}
    }
    
    if (repair_joint_rd_key(joint, &d_key, data)) {
	aal_exception_error("Node (%llu): Failed to get the right delimiting "
	    "key.", joint->node->blk);
	return -1;
    }

    pos.item = reiser4_node_count(joint->node) - 1;
    pos.unit = ~0ul;
 
    if (reiser4_coord_open(&coord, joint, CT_JOINT, &pos)) {
	aal_exception_error("Node (%llu): Failed to open the item (%llu).",
	    joint->node->blk, pos.item);
	return -1;
    }
    
    if (reiser4_item_max_real_key(&coord, &key)) {
	aal_exception_error("Node (%llu): Failed to get the max real key of "
	    "the last item.", joint->node->blk);
	return -1;
    }
    
    if (reiser4_key_compare(&key, &d_key) > 0) {
	aal_exception_error("Node (%llu): The last key %k in the node is less "
	    "then the right delimiting key %k.", 
	    joint->node->blk, &key, &d_key);
	return 1;
    }

    return 0;
}

static errno_t repair_node_keys_check(reiser4_node_t *node, 
    repair_data_t *data) 
{
    reiser4_key_t key, prev_key;
    reiser4_pos_t pos = {0, ~0ul};
    errno_t res;
    
    aal_assert("vpf-258", node != NULL, return -1);
    aal_assert("vpf-266", data != NULL, return -1);
    
    if (!(key.plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, 
	KEY_REISER40_ID))) 
    {
	aal_exception_error("Can't find key plugin by its id 0x%x.", 
	    KEY_REISER40_ID);
	return -1;
    }

    for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++) {
	reiser4_coord_t coord;

	if (reiser4_coord_open(&coord, node, CT_NODE, &pos))
		return -1;
/*	if (reiser4_node_get_key(node, &pos, &key)) {*/
	if (reiser4_item_key(&coord, &key)) {
	    aal_exception_error("Node (%llu): Failed to get the key of the "
		"item (%u).", node->blk, pos.item);
	    return -1;
	}
	
	if (reiser4_key_valid(&key)) {
	    aal_exception_error("Node (%llu): The key %k of the item (%u) is "
		"not valid. Item removed.", node->blk, &key, pos.item);
	    
	    if (reiser4_node_remove(node, &pos)) {
		aal_exception_bug("Node (%llu): Failed to delete the item "
		    "(%d).", node->blk, pos.item);
		return -1;
	    }
	    pos.item--;
	}
	
	if (pos.item) {	    
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
errno_t repair_joint_check(reiser4_joint_t *joint, repair_data_t *data) {
    int res;
    
    aal_assert("vpf-183", data != NULL, return -1);
    aal_assert("vpf-192", joint != NULL, return -1);
    aal_assert("vpf-494", joint->node != NULL, return -1);
    aal_assert("vpf-193", joint->node->entity != NULL, return -1);    
    aal_assert("vpf-220", joint->node->entity->plugin != NULL, return -1);

    if ((res = plugin_call(return -1, joint->node->entity->plugin->node_ops, 
	check, joint->node->entity)))
	return res;

    if ((res = repair_node_items_check(joint->node, data))) 
	return res;
    
    if ((res = repair_node_keys_check(joint->node, data)))
	return res;
 
    if (reiser4_node_count(joint->node) == 0)
	return 1;

    return 0;
}


