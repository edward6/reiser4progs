/* 
 * librepair/filter.c - methods are needed for the fsck pass1. 
 * Copyright (C) 1996-2002 Hans Reiser.
 * The first fsck pass - filter - fsck filters corrupted parts of 
 * a reiser4 tree out, repairs all recoverable corruptions, builds
 * a map of leaves. 
*/

#include <repair/librepair.h>

/* Open callback for traverse. It opens a node at passed blk, creates a joint 
 * on it. It does nothing if REPAIR_BAD_POINTER is set and set this flag if 
 * node cannot be opeened. Returns error if any. */
errno_t repair_filter_joint_open(reiser4_joint_t **joint, blk_t blk, 
    traverse_hint_t *hint)
{
    errno_t res = 0;
    aal_device_t *device;
    reiser4_node_t *node;
    repair_data_t *repair_data = (repair_data_t *)hint->data;

    aal_assert("vpf-379", repair_data != NULL, return -1);
    aal_assert("vpf-432", joint != NULL, return -1);
    aal_assert("vpf-433", repair_data->format != NULL, return -1);

    if (repair_test_flag(repair_data, REPAIR_BAD_PTR))
	return 0;

    device = repair_data->format->device;
    
    if ((node = reiser4_node_open(device, blk)) == NULL) {
	repair_set_flag(repair_data, REPAIR_BAD_PTR);
	return 0;
    }
	    
    if (!(*joint = reiser4_joint_create(node))) 
	goto error_free_node;

    return 0;
    
error_free_node:
    reiser4_node_close(node);
    return -1;
}

/* Before callback for traverse. It checks node level, node consistency, and 
 * delimiting keys. If any check reveals a problem with the data consistency
 * it sets REPAIR_NOT_FIXED flag. */
errno_t repair_filter_joint_check(reiser4_joint_t *joint, traverse_hint_t *hint) {
    repair_filter_data_t *filter_data;
    object_entity_t *entity;
    errno_t res = 0;
    
    aal_assert("vpf-252", hint  != NULL, return -1);
    aal_assert("vpf-409", joint != NULL, return -1);
    aal_assert("vpf-410", joint->node != NULL, return -1);
    aal_assert("vpf-411", joint->node->entity != NULL, return -1);    
    aal_assert("vpf-412", joint->node->entity->plugin != NULL, return -1);

    filter_data = repair_filter_data((repair_data_t *)hint->data);
    entity = joint->node->entity;

    /* Skip this check if level is not set (root node only). */
    if (entity->plugin->node_ops.get_level && filter_data->level && 
	entity->plugin->node_ops.get_level(entity) != filter_data->level) 
    {
	aal_exception_error("Level of the node (%u) mismatches to the expected "
	    "one (%u).", entity->plugin->node_ops.get_level(entity), 
	    filter_data->level);
	res = 1;
    }

    if (!res && (res = repair_joint_check(joint, hint->data)) < 0)
	return res;
	
    if (!res && (res = repair_joint_dkeys_check(joint, hint->data)) < 0)
	return res;

    if (res > 0)
	repair_set_flag((repair_data_t *)hint->data, REPAIR_NOT_FIXED);

    return res;
}

/* Setup callback for traverse. Prepares essential information for a child of 
 * a node - level, mark node as used in the bm_formatted bitmap. If block was 
 * met already, set REPAIR_BAD_PTR flag. */
errno_t repair_filter_setup_traverse(reiser4_coord_t *coord, traverse_hint_t *hint) {
    reiser4_ptr_hint_t ptr;
    repair_data_t *repair_data = (repair_data_t *)hint->data;
	
    aal_assert("vpf-255", repair_data != NULL, return -1);
    aal_assert("vpf-254", repair_data->format != NULL, return -1);
    aal_assert("vpf-269", coord != NULL, return -1);
    
    /* Mark pointed block as used in the formatted bitmap. */
    if (plugin_call(return -1, coord->entity.plugin->item_ops,
	fetch, &coord->entity, coord->pos.unit, &ptr, 1))
	return -1;
    
    if (aux_bitmap_test(repair_filter_data(repair_data)->bm_formatted, ptr.ptr)) 
    {
	/* This block was met more then once in the formatted part of the tree. */
	repair_set_flag(repair_data, REPAIR_BAD_PTR);
    } else {	
	/* We meet the block for the first time. */
	aux_bitmap_mark(repair_filter_data(repair_data)->bm_formatted, ptr.ptr);
    }

/* this seems to be done at node_check time.
    if ((res = repair_coord_ptr_check(coord, repair_data)) > 0) {
	repair_set_flag(repair_data, REPAIR_BAD_PTR);
	res = 0;
    }
*/

    /* Initialize the level for the root node before traverse. */
    /* FIXME-VITALY: If there is no get_level method implemented we have to 
     * rely on the height in super block what is not very well. What if it is 
     * corrupted in the SB? And == 0? I would prefere to set it up when the 
     * first leaf is found. But how is it possible to identify the leaf if 
     * there is no level in it? */
    if (!repair_filter_data(repair_data)->level)
	repair_filter_data(repair_data)->level = 
	    (reiser4_coord_entity(coord)->plugin->node_ops.get_level ? 
	    reiser4_coord_entity(coord)->plugin->node_ops.get_level(
		reiser4_coord_entity(coord)) : 
	    reiser4_format_get_height(repair_data->format));
    
    repair_filter_data(repair_data)->level--;
    
    return 0;
}

/* Update callback for traverse. It rollback changes made in setup_traverse
 * callback and do some essential stuff after traversing through the child -
 * level, if REPAIR_NOT_FIXED flag is set - deletes the child pointer and 
 * mark the pointed block as unused in bm_formatted bitmap. */
errno_t repair_filter_update_traverse(reiser4_coord_t *coord, traverse_hint_t *hint) {
    reiser4_pos_t prev;
    repair_data_t *repair_data = (repair_data_t *)hint->data;
    
    aal_assert("vpf-257", repair_data != NULL, return -1);
    aal_assert("vpf-434", coord != NULL, return -1);
    
    if (repair_test_flag(repair_data, REPAIR_NOT_FIXED)) {
	reiser4_ptr_hint_t ptr;
	
	/* Clear pointed block in the formatted bitmap. */
	if (plugin_call(return -1, coord->entity.plugin->item_ops,
	    fetch, &coord->entity, coord->pos.unit, &ptr, 1))
	    return -1;
	
	aux_bitmap_clear(repair_filter_data(repair_data)->bm_formatted, ptr.ptr);
	
	/* The node corruption was not fixed - delete the internal item. */
	repair_coord_left_pos_save(coord, &prev);
	if (reiser4_node_remove(reiser4_coord_node(coord), &coord->pos)) {
	    aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.", 
		reiser4_coord_blk(coord), coord->pos.item, coord->pos.unit);
	    return -1;
	}
	coord->pos = prev;
	repair_clear_flag(repair_data, REPAIR_NOT_FIXED);
    } 
    
    repair_filter_data(repair_data)->level++;

    return 0;
}

/* After callback for traverse. Does needed stuff after traversing through all 
 * children - if no child left, set REPAIR_NOT_FIXED flag to forse deletion of 
 * the pointer to this block in update_traverse callback. */
errno_t repair_filter_after_traverse(reiser4_joint_t *joint, traverse_hint_t *hint) {
    repair_data_t *repair_data = (repair_data_t *)hint->data;
     
    aal_assert("vpf-393", joint != NULL, return -1);
    aal_assert("vpf-394", joint->node != NULL, return -1);   
    aal_assert("vpf-256", repair_data != NULL, return -1);    

    if (reiser4_node_count(joint->node) == 0)
	repair_set_flag(repair_data, REPAIR_NOT_FIXED);
    /* FIXME-VITALY: else - sync the node */

    return 0;
}

/* Setup data (common and specific) before traverse through the tree. */
errno_t repair_filter_setup(traverse_hint_t *hint) {
    repair_data_t *repair_data;    
    
    aal_assert("vpf-420", hint != NULL, return -1);
    aal_assert("vpf-423", hint->data != NULL, return -1);
    
    repair_data = hint->data;

    /* Allocate a bitmap for formatted blocks in the tree. */
    if (!(repair_filter_data(repair_data)->bm_formatted = aux_bitmap_create(
	reiser4_format_get_len(repair_data->format)))) 
    {
	aal_exception_error(
	    "Failed to allocate a bitmap for once pointed blocks.");
	return -1;
    }
    
        /* Hint for objects to be traversed - node pointers only here. */
    hint->objects = 1 << NODEPTR_ITEM;
    
    repair_data->flags = 0;
    
    return 0;
}

/* Does some updata stuff after traverse through the internal tree - deletes 
 * the pointer to the root block from the specific super block if 
 * REPAIR_NOT_FIXED flag is set, mark that block used in bm_formatted bitmap 
 * otherwise. */
errno_t repair_filter_update(traverse_hint_t *hint) {
    repair_data_t *repair_data;

    aal_assert("vpf-421", hint != NULL, return -1);
    aal_assert("vpf-422", hint->data != NULL, return -1);
    
    repair_data = hint->data;
    
    if (repair_test_flag(repair_data, REPAIR_NOT_FIXED)) {
	reiser4_format_set_root(repair_data->format, FAKE_BLK);
	repair_clear_flag(repair_data, REPAIR_NOT_FIXED);
    } else {
	/* Mark the root block as a formatted block in the bitmap. */
	aux_bitmap_mark(repair_filter_data(repair_data)->bm_formatted, 
	    reiser4_format_get_root(repair_data->format));
    }

    return 0;
}

