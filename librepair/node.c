/*
    librepair/node.c - methods are needed for node recovery.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_node_items_check(reiser4_node_t *node, 
    repair_check_t *data) 
{
    reiser4_item_t item;
    reiser4_pos_t pos;
    rpid_t pid;
    int res;

    aal_assert("vpf-229", node != NULL, return -1);
    aal_assert("vpf-230", node->entity != NULL, return -1);
    aal_assert("vpf-231", node->entity->plugin != NULL, return -1);
    aal_assert("vpf-242", data != NULL, return -1);

    pos.item = reiser4_node_count(node) - 1;
    do {
	pos.unit = ~0ul;
	
	/* Open the item, checking its plugin id. */
	if ((res = repair_item_open(&item, node, &pos))) {
	    if (res > 0) {
		aal_exception_error("Node (%llu): Failed to open the item (%u)."
		    " Removed.", aal_block_number(node->block), pos.item);
	    
		if (reiser4_node_remove(node, &pos)) {
		    aal_exception_bug("Node (%llu): Failed to delete the item "
			"(%d).", aal_block_number(node->block), pos.item);
		    return -1;
		}
		
		continue;		
	    } 
		
	    aal_exception_error("Node (%llu): Failed to open the item (%u).", 
		aal_block_number(node->block), pos.item);

	    return res;
	}
	
	/* Check that the item is legal for this node. If not, it will be deleted 
	 * in update traverse callback method. */
	if ((res = plugin_call(return -1, node->entity->plugin->node_ops, 
	    item_legal, node->entity, item.plugin)))
	    return res;	

	/* Check the item structure. */
	if ((res = plugin_call(return -1, item.plugin->item_ops, check, 
	    &item, data->options))) 
	    return res;

	if (!reiser4_item_extent(&item) && !reiser4_item_nodeptr(&item))
	    continue;
	
	pos.unit = reiser4_item_count(&item) - 1;
	
	do {
	    if ((res = repair_item_nptr_check(node, &item, data)) < 0) 
		return -1;
	    else {
		reiser4_ptr_hint_t hint;
			
		if (plugin_call(return -1, item.plugin->item_ops,
						fetch, &item, 0, &hint, 1))
			return -1;
		
		if (reiser4_item_nodeptr(&item)) {
		    aal_exception_error("Node (%llu), item (%u), unit (%u): "
			"bad internal pointer (%llu/%llu). Removed.", 
			aal_block_number(node->block), pos.item, pos.unit, 
			hint.ptr, 0);

		    if (reiser4_node_remove(node, &pos))
			return -1;
		} else if (reiser4_item_extent(&item)) {
		    aal_exception_error("Node (%llu), item (%u), unit (%u): "
			"bad extent pointer (%llu). Zeroed.", 
			aal_block_number(node->block), pos.item, pos.unit, 
			hint.ptr, hint.width);

			hint.ptr = 0;
			hint.width = 0;
			
			if (plugin_call(return -1, item.plugin->item_ops,
				update, &item, 0, &hint, 1))
				return -1;
		}
	    }
	} while (pos.unit--);	
    } while (pos.item--);
    
    return 0;    
}

errno_t repair_joint_ld_key(reiser4_joint_t *joint, reiser4_key_t *ld_key, 
    repair_check_t *data) 
{
    reiser4_item_t item;
    errno_t res;
    
    aal_assert("vpf-393", joint != NULL, return -1);
    aal_assert("vpf-344", ld_key != NULL, return -1);
    aal_assert("vpf-345", data != NULL, return -1);

    if (joint->parent != NULL) {
	if ((res = reiser4_item_open(&item, joint->parent->node->entity, 
	    &joint->pos)))
	    return res;
	
	return reiser4_item_get_key(&item, ld_key);
    }

    reiser4_key_minimal(ld_key);
    
    return 0;
}

errno_t repair_joint_rd_key(reiser4_joint_t *joint, reiser4_key_t *rd_key, 
    repair_check_t *data)
{
    reiser4_item_t item;
    reiser4_pos_t pos = {0, 0};
    errno_t res;
    
    aal_assert("vpf-394", joint != NULL, return -1);
    aal_assert("vpf-347", rd_key != NULL, return -1);
    aal_assert("vpf-348", data != NULL, return -1);

    if (joint->parent != NULL) {
	if ((res = reiser4_item_open(&item, joint->parent->node->entity, 
	    &joint->pos)))
	    return res;
	
	if (reiser4_node_count(joint->node) == item.pos->item + 1) {
	    return repair_joint_rd_key(joint->parent, rd_key, data);
	} else {
	    pos.item = item.pos->item + 1;
	    reiser4_item_open(&item, joint->parent->node->entity, &pos);
	    return reiser4_item_get_key(&item, rd_key);
	}
    }

    reiser4_key_maximal(rd_key);
    
    return 0;

}

/* 
    FIXME-VITALY: Should this stuff be moved to plugin and how will 3.6 format be 
    supported? 
*/
static errno_t repair_joint_dkeys_check(reiser4_joint_t *joint, 
    repair_check_t *data) 
{
    reiser4_key_t key, d_key;
    reiser4_pos_t pos;
    reiser4_item_t item;

    aal_assert("vpf-248", joint != NULL, return -1);
    aal_assert("vpf-395", joint->node != NULL, return -1);
    aal_assert("vpf-249", joint->node->entity != NULL, return -1);
    aal_assert("vpf-250", joint->node->entity->plugin != NULL, return -1);
    aal_assert("vpf-240", data != NULL, return -1);
    aal_assert("vpf-241", data->format != NULL, return -1);

    if (repair_joint_ld_key(joint, &d_key, data)) {
	aal_exception_error("Node (%llu): Failed to get the left delimiting key.", 
	    aal_block_number(joint->node->block));
	return -1;
    }
    
    reiser4_pos_init(&pos, 0, ~0ul);
	
    if (reiser4_item_open(&item, joint->node->entity, &pos))
	return -1;
	
    if (reiser4_item_get_key(&item, &key)) {
	aal_exception_error("Node (%llu): Failed to get the left key.",
	    aal_block_number(joint->node->block));
	return -1;
    }

    if (reiser4_key_compare(&d_key, &key) != 0) {
	aal_exception_error("Node (%llu): The first key %k is not equal to "
	    "the left delimiting key %k.", aal_block_number(joint->node->block), 
	    &key, &d_key);
	return 1;
    }
    
    if (repair_joint_rd_key(joint, &d_key, data)) {
	aal_exception_error("Node (%llu): Failed to get the right delimiting "
	    "key.", aal_block_number(joint->node->block));
	return -1;
    }

    reiser4_pos_init(&pos, reiser4_node_count(joint->node) - 1, ~0ul);
 
    if (reiser4_item_open(&item, joint->node->entity, &pos)) {
	aal_exception_error("Node (%llu): Failed to open the item (%llu).",
	    aal_block_number(joint->node->block), pos.item);
	return -1;
    }
    
    if (reiser4_item_max_real_key(&item, &key)) {
	aal_exception_error("Node (%llu): Failed to get the max real key of "
	    "the last item.", aal_block_number(joint->node->block));
	return -1;
    }
    
    if (reiser4_key_compare(&key, &d_key) < 0) {
	aal_exception_error("Node (%llu): The last key %k in the node is less "
	    "then the right delimiting key %k.", 
	    aal_block_number(joint->node->block), &key, &d_key);
	return 1;
    }

    return 0;
}

static errno_t repair_node_keys_check(reiser4_node_t *node, 
    repair_check_t *data) 
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
    
    pos.item = reiser4_node_count(node) - 1;
    do {
		reiser4_item_t item;
		if (reiser4_item_open(&item, node->entity, &pos))
			return -1;
/*	if (reiser4_node_get_key(node, &pos, &key)) {*/
	if (reiser4_item_get_key(&item, &key)) {
	    aal_exception_error("Node (%llu): Failed to get the key of the "
		"item (%u).", aal_block_number(node->block), pos.item);
	    return -1;
	}
	
	if (reiser4_key_valid(&key)) {
	    aal_exception_error("Node (%llu): The key %k of the item (%u) is "
		"not valid. Item removed.", aal_block_number(node->block), 
		&key, pos.item);
	    
	    if (reiser4_node_remove(node, &pos)) {
		aal_exception_bug("Node (%llu): Failed to delete the item "
		    "(%d).", aal_block_number(node->block), pos.item);
		return -1;
	    }
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
    } while (pos.item--);
    
    return 0;
}

/*  
    Checks the node content. 
    Returns: 0 - OK; -1 - unexpected error; 1 - unrecoverable error;

    Supposed to be run with repair_check.pass.filter structure initialized.
*/
errno_t repair_joint_check(reiser4_joint_t *joint, repair_check_t *data) {
    int res;
    
    aal_assert("vpf-183", data != NULL, return -1);
    aal_assert("vpf-184", data->format != NULL, return -1);
    aal_assert("vpf-192", joint != NULL, return -1);
    aal_assert("vpf-192", joint->node != NULL, return -1);
    aal_assert("vpf-193", joint->node->entity != NULL, return -1);    
    aal_assert("vpf-220", joint->node->entity->plugin != NULL, return -1);

    /* Skip this check if level is not set. level is not set for the root node.*/
    if (joint->node->entity->plugin->node_ops.get_level && 
	repair_filter_data(data)->level && 
	joint->node->entity->plugin->node_ops.get_level(joint->node->entity) != 
	repair_filter_data(data)->level) 
    {
	aal_exception_error("Level of the node (%u) is not correct, expected (%u)", 
	    joint->node->entity->plugin->node_ops.get_level(joint->node->entity), 
	    repair_filter_data(data)->level);
	return 1;
    }

    /* Check if we met it already in the control allocator. */
/*  FIXME-VITALY: This will be moved to scan pass.
    if (reiser4_alloc_test(data->a_control, aal_block_number(node->block))) 
    {
	aal_exception_error("The node in the block (%llu) is used more then "
	    "once in the tree.", aal_block_number(node->block));
	return 1;
    }
*/
    if ((res = plugin_call(return -1, joint->node->entity->plugin->node_ops, 
	check, joint->node->entity, data->options)))
	return res;

    if ((res = repair_node_items_check(joint->node, data))) 
	return res;
    
    if ((res = repair_node_keys_check(joint->node, data)))
	return res;
 
    if ((res = repair_joint_dkeys_check(joint, data)))
	return res;
 
    if (reiser4_node_count(joint->node) == 0)
	return 1;

    return 0;
}

/* 
    Zero extent pointers which point to an already used block. 
    Returns -1 if block is used already.
*/
errno_t repair_node_handle_pointers(reiser4_node_t *node, repair_check_t *data) 
{
    reiser4_item_t item;
    reiser4_pos_t pos = {0, 0};
    
    aal_assert("vpf-384", node != NULL, return -1);
    aal_assert("vpf-385", data != NULL, return -1);
    aal_assert("vpf-386", !aux_bitmap_test(repair_scan_data(data)->formatted, 
	aal_block_number(node->block)), return -1);

    aux_bitmap_mark(repair_scan_data(data)->formatted, 
	aal_block_number(node->block));
    aux_bitmap_clear(repair_scan_data(data)->used, 
	aal_block_number(node->block));
   
    for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++)  {	
	if (repair_item_open(&item, node, &pos)) {
	    aal_exception_error("Node (%llu): failed to open the item (%u).", 
		aal_block_number(node->block), pos.item);
	    return -1;
	}	    

	if (!reiser4_item_extent(&item) && !reiser4_item_nodeptr(&item))
	    continue;

	for (pos.unit = 0; pos.unit < reiser4_item_count(&item); pos.unit++) {
		reiser4_ptr_hint_t ptr;
	    blk_t form_blk, used_blk;

		if (plugin_call(return -1, item.plugin->item_ops, fetch,
						&item, 0, &ptr, 1))
			return -1;
		
	    aal_assert("vpf-387", 
		(ptr.ptr < reiser4_format_get_len(data->format)) && 
		(ptr.width < reiser4_format_get_len(data->format)) && 
		(ptr.ptr + ptr.width < reiser4_format_get_len(data->format)), 
		return -1);
	}
    }
    
    return 0;
}
