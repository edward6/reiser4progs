/*
    librepair/filter.c - methods are needed for the fsck pass1. 
    Copyright (C) 1996-2002 Hans Reiser.

    The first fsck pass - filter - fsck filters corrupted parts of 
    a reiser4 tree out, repairs all recoverable corruptions, builds
    a map of leaves. 
*/

#include <repair/librepair.h>

errno_t repair_filter_joint_open(reiser4_joint_t **joint, blk_t blk, 
    void *data)
{
    errno_t res = 0;
    aal_device_t *device;
    reiser4_node_t *node;
    repair_check_t *check_data = data;

    aal_assert("vpf-379", check_data != NULL, return -1);
    aal_assert("vpf-432", joint != NULL, return -1);
    aal_assert("vpf-433", check_data->format != NULL, return -1);

    if (repair_test_flag(check_data, REPAIR_BAD_PTR))
	return 0;

    device = check_data->format->device;
    
    if ((node = reiser4_node_open(device, blk)) == NULL) {
	repair_set_flag(check_data, REPAIR_BAD_PTR);
	return 0;
    }
	    
    if (!(*joint = reiser4_joint_create(node))) 
	goto error_free_node;

    return 0;
    
error_free_node:
    reiser4_node_close(node);
    return -1;
}

errno_t repair_filter_joint_check(reiser4_joint_t *joint, void *data) {
    repair_check_t *check_data = data;
    errno_t res = 0;
    
    aal_assert("vpf-252", check_data != NULL, return -1);
    aal_assert("vpf-409", joint != NULL, return -1);
    aal_assert("vpf-410", joint->node != NULL, return -1);
    aal_assert("vpf-411", joint->node->entity != NULL, return -1);    
    aal_assert("vpf-412", joint->node->entity->plugin != NULL, return -1);

    /* Skip this check if level is not set. level is not set for the root node.*/
    if (joint->node->entity->plugin->node_ops.get_level && 
	repair_filter_data(check_data)->level && 
	joint->node->entity->plugin->node_ops.get_level(joint->node->entity) != 
	repair_filter_data(check_data)->level) 
    {
	aal_exception_error("Level of the node (%u) is not correct, expected (%u)", 
	    joint->node->entity->plugin->node_ops.get_level(joint->node->entity), 
	    repair_filter_data(check_data)->level);
	res = 1;
    }

    if (!res && (res = repair_joint_check(joint, check_data)) < 0)
	return res;
	
    if (!res && repair_filter_data(check_data)->level)
	if ((res = repair_joint_dkeys_check(joint, data)) < 0)
	    return res;

    if (res > 0)
	repair_set_flag(check_data, REPAIR_NOT_FIXED);
	
    return res;
}

errno_t repair_filter_setup_traverse(reiser4_coord_t *coord, void *data) {
    repair_check_t *check_data = data;
    reiser4_ptr_hint_t ptr;
	
    aal_assert("vpf-255", check_data != NULL, return -1);
    aal_assert("vpf-254", check_data->format != NULL, return -1);
    aal_assert("vpf-269", coord != NULL, return -1);
    
    /* Clear bit in the formatetd bitmap. */
    if (plugin_call(return -1, coord->entity.plugin->item_ops,
	fetch, &coord->entity, coord->pos.unit, &ptr, 1))
	return -1;
    
    if (!aux_bitmap_test(repair_filter_data(check_data)->formatted, ptr.ptr)) {
	/* This block was met more then once in the formatted part of the tree. */
	repair_set_flag(check_data, REPAIR_BAD_PTR);
    } else {	
	/* We meet the block for the first time. */
	aux_bitmap_clear(repair_filter_data(check_data)->formatted, ptr.ptr);
    }

/* this seems to be done at node_check time.
    if ((res = repair_coord_ptr_check(coord, check_data)) > 0) {
	repair_set_flag(check_data, REPAIR_BAD_PTR);
	res = 0;
    }
*/

    /* Initialize the level for the root node before traverse. */
    if (!repair_filter_data(check_data)->level)
	repair_filter_data(check_data)->level = 
	    (reiser4_coord_entity(coord)->plugin->node_ops.get_level ? 
	    reiser4_coord_entity(coord)->plugin->node_ops.get_level(
		reiser4_coord_entity(coord)) : 
	    reiser4_format_get_height(check_data->format));
    
    repair_filter_data(check_data)->level--;
    
    return 0;
}

errno_t repair_filter_update_traverse(reiser4_coord_t *coord, void *data) {
    repair_check_t *check_data = data;
    reiser4_pos_t prev;
    
    aal_assert("vpf-257", check_data != NULL, return -1);
    aal_assert("vpf-434", coord != NULL, return -1);
    
    if (repair_test_flag(check_data, REPAIR_NOT_FIXED)) {
	reiser4_ptr_hint_t ptr;
	
	/* Set the bit in the formatted bitmap back. */
	if (plugin_call(return -1, coord->entity.plugin->item_ops,
	    fetch, &coord->entity, coord->pos.unit, &ptr, 1))
	    return -1;
	
	aux_bitmap_mark(repair_filter_data(check_data)->formatted, ptr.ptr);
	
	/* The node corruption was not fixed - delete the internal item. */
	repair_coord_left_pos_save(coord, &prev);
	if (reiser4_node_remove(reiser4_coord_node(coord), &coord->pos)) {
	    aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.", 
		aal_block_number(reiser4_coord_block(coord)), coord->pos.item, 
		coord->pos.unit);
	    return -1;
	}
	coord->pos = prev;
	repair_clear_flag(check_data, REPAIR_NOT_FIXED);
    } 
    
    repair_filter_data(check_data)->level++;

    return 0;
}

errno_t repair_filter_after_traverse(reiser4_joint_t *joint, void *data) {
    repair_check_t *check_data = data;
     
    aal_assert("vpf-393", joint != NULL, return -1);
    aal_assert("vpf-394", joint->node != NULL, return -1);   
    aal_assert("vpf-256", check_data != NULL, return -1);
    

    if (reiser4_node_count(joint->node) == 0)
	repair_set_flag(check_data, REPAIR_NOT_FIXED);
    /* FIXME-VITALY: else - sync the node */

    return 0;
}

/* Setup data and initialize data->pass.filter. */
errno_t repair_filter_setup(reiser4_fs_t *fs, traverse_hint_t *hint) {
    repair_check_t *check_data;    
    
    aal_assert("vpf-420", hint != NULL, return -1);
    aal_assert("vpf-423", hint->data != NULL, return -1);
    
    check_data = hint->data;
   
    check_data->format = fs->format;
    check_data->options = repair_data(fs)->options;

    if (!(repair_filter_data(check_data)->formatted = aux_bitmap_create(
	reiser4_format_get_len(check_data->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for once pointed blocks.");
	return -1;
    }
    
    aal_memset(repair_filter_data(check_data)->formatted->map, 0xff, 
	repair_filter_data(check_data)->formatted->size);
    
    if (!(repair_filter_data(check_data)->format_layout = aux_bitmap_create(
	reiser4_format_get_len(check_data->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for format layout blocks.");
	return -1;
    }
 
    aal_memset(repair_filter_data(check_data)->format_layout->map, 0xff, 
	repair_filter_data(check_data)->format_layout->size);
    
    if (reiser4_format_layout(fs->format, callback_mark_format_block, 
	repair_filter_data(check_data)->format_layout)) 
    {
	aal_exception_error("Failed to mark all format blocks in the bitmap as unused.");
	return -1;
    }
    
    hint->objects = 1 << NODEPTR_ITEM;
    
    return 0;
}

errno_t repair_filter_update(reiser4_fs_t *fs, traverse_hint_t *hint) {
    repair_check_t *check_data;

    aal_assert("vpf-421", hint != NULL, return -1);
    aal_assert("vpf-422", hint->data != NULL, return -1);
    aal_assert("vpf-422", hint->data != NULL, return -1);
    
    check_data = hint->data;
    
    if (repair_test_flag(check_data, REPAIR_NOT_FIXED)) {
	reiser4_format_set_root(check_data->format, FAKE_BLK);
	repair_clear_flag(check_data, REPAIR_NOT_FIXED);
    } else {
	/* Mark the root block as a formatted block in the bitmap. */
	aux_bitmap_clear(repair_filter_data(check_data)->formatted, 
	    reiser4_format_get_root(check_data->format));
    }

    return 0;
}

