/*
    librepair/cut_tree.c - methods are needed for the fsck pass1. 
    Copyright (C) 1996-2002 Hans Reiser.

    The first fsck pass - cut tree - fsck cuts corrupted parts of 
    a reiser4 tree off, repairs all recoverable corruptions, builds
    a map of leaves. 
*/

#include <repair/librepair.h>

errno_t repair_cut_tree_node_open(reiser4_node_t *node, blk_t blk, 
    void *data)
{
    repair_check_t *check_data = data;
    aal_block_t *block;
    errno_t res;

    aal_assert("vpf-379", check_data != NULL, return -1);

    if (!aux_bitmap_test(repair_cut_data(check_data)->format_layout, blk)) {
	aal_exception_error("Node (%llu): format area cannot contain a node.", 
	    blk);

	repair_set_flag(check_data, REPAIR_NOT_FIXED);
	return 1;
    }
    
    return repair_node_open(node, blk, data);
}

errno_t repair_cut_tree_before_traverse(reiser4_node_t *node, reiser4_item_t *item, 
    void *data) 
{
    repair_check_t *check_data = data;
    
    aal_assert("vpf-251", node != NULL, return -1);
    aal_assert("vpf-253", check_data != NULL, return -1);
    aal_assert("vpf-254", check_data->format != NULL, return -1);

    /* Initialize the level for the root node before traverse. */
    if (!repair_cut_data(check_data)->level)
	repair_cut_data(check_data)->level = 
	    (node->entity->plugin->node_ops.get_level ? 
	    node->entity->plugin->node_ops.get_level(node->entity) : 
	    reiser4_format_get_height(check_data->format));
    
    repair_cut_data(check_data)->level--;

    repair_cut_data(check_data)->nodes_path = 
	aal_list_append(repair_cut_data(check_data)->nodes_path, node);
    repair_cut_data(check_data)->items_path = 
	aal_list_append(repair_cut_data(check_data)->items_path, item);

    return 0;
}

errno_t repair_cut_tree_after_traverse(reiser4_node_t *node, reiser4_item_t *item, 
    void *data) 
{
    repair_check_t *check_data = data;
    
    aal_assert("vpf-256", check_data != NULL, return -1);
    
    repair_cut_data(check_data)->level++;

    if (reiser4_node_count(node) == 0)
	repair_set_flag(check_data, REPAIR_NOT_FIXED);

    aal_list_remove(repair_cut_data(check_data)->nodes_path, 
	aal_list_last(repair_cut_data(check_data)->nodes_path)->data);
    aal_list_remove(repair_cut_data(check_data)->items_path, 
	aal_list_last(repair_cut_data(check_data)->items_path)->data);
    
    return 0;
}

errno_t repair_cut_tree_setup_traverse(reiser4_node_t *node, 
    reiser4_item_t *item, void *data)
{
    repair_check_t *check_data = data;
    blk_t target;
    int res;
    
    aal_assert("vpf-255", data != NULL, return -1);
    aal_assert("vpf-269", item != NULL, return -1);

    if ((res = repair_item_nptr_check(node, item, check_data)))
	return res;

    return 0;
}

errno_t repair_cut_tree_update_traverse(reiser4_node_t *node, 
    reiser4_item_t *item, void *data) 
{
    repair_check_t *check_data = data;
    
    aal_assert("vpf-257", check_data != NULL, return -1);
    
    if (repair_test_flag(check_data, REPAIR_NOT_FIXED)) {
	/* The node corruption was not fixed - delete the internal item. */
	if (reiser4_node_remove(node, item->pos)) {
	    aal_exception_error("Node (%llu), pos (%u, %u): Remove failed.", 
		aal_block_number(node->block), item->pos->item, item->pos->unit);
	    return -1;
	}
	repair_clear_flag(check_data, REPAIR_NOT_FIXED);
    } else {
	/* Mark the child as a formatted block in the bitmap. */
	aux_bitmap_clear(repair_cut_data(check_data)->formatted, 
	    reiser4_item_get_nptr(item));
    }
    
    return 0;
}

errno_t repair_cut_tree_node_check(reiser4_node_t *node, void *data) {
    repair_check_t *check_data = data;
    errno_t res;
    
    aal_assert("vpf-252", check_data != NULL, return -1);
    
    if ((res = repair_node_check(node, check_data)) > 0) 
	repair_set_flag(check_data, REPAIR_NOT_FIXED);
    else 
	return res;
        
    return 0;
}

errno_t repair_cut_tree_setup(reiser4_fs_t *fs, repair_check_t *data) {
/*  Will be needed on scan pass.
    if (!(data->a_control = reiser4_alloc_create(fs->format, 
	reiser4_format_get_len(fs->format)))) 
    {
	aal_exception_fatal("Failed to create a control allocator.");
	return -1;
    }
        
    if (!(data->oid_control = reiser4_oid_create(fs->format))) {
	aal_exception_fatal("Failed to create a control oid allocator.");
	return -1;
    }
    
   // Mark the format area as used in the control allocator
    reiser4_format_mark(fs->format, data->a_control);
*/
 
    if (!(repair_cut_data(data)->formatted = aux_bitmap_create(
	reiser4_format_get_len(data->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for once pointed blocks.");
	return -1;
    }
    
    aal_memset(repair_cut_data(data)->formatted->map, 0xff, 
	repair_cut_data(data)->formatted->size);
    
    if (!(repair_cut_data(data)->format_layout = aux_bitmap_create(
	reiser4_format_get_len(data->format)))) 
    {
	aal_exception_error("Failed to allocate a bitmap for format layout blocks.");
	return -1;
    }
    
    aal_memset(repair_cut_data(data)->format_layout->map, 0xff, 
	repair_cut_data(data)->format_layout->size);
    
    if (reiser4_format_layout(fs->format, callback_mark_format_block, 
	repair_cut_data(data)->format_layout)) 
    {
	aal_exception_error("Failed to mark all format blocks in the bitmap as unused.");
	return -1;
    }
    
    data->format = fs->format;
    data->options = repair_data(fs)->options;

    return 0;
}

errno_t repair_cut_tree_update(reiser4_fs_t *fs, repair_check_t *data) {    
    if (repair_test_flag(data, REPAIR_NOT_FIXED)) {
	reiser4_format_set_root(data->format, ~0ull);
	repair_clear_flag(data, REPAIR_NOT_FIXED);
    } else {
	/* Mark the root block as a formatted block in the bitmap. */
	aux_bitmap_clear(repair_cut_data(data)->formatted, 
	    reiser4_format_get_root(data->format));
    }

    return 0;
}

