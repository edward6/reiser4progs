/*
    librepair/item.c -- common repair item functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_item_nptr_check(reiser4_node_t *node, 
    reiser4_item_t *item, repair_check_t *data) 
{
    blk_t ptr, next_blk;
    count_t width;
    int res;

    aal_assert("vpf-368", node != NULL, return -1);
    aal_assert("vpf-382", node->block != NULL, return -1);
    aal_assert("vpf-270", item != NULL, return -1);
    aal_assert("vpf-271", data != NULL, return -1);
    aal_assert("vpf-272", data->format != NULL, return -1);

    /*
	FIXME-VITALY: This stuff should be tested carefully when functions 
	return 0 as an error. 
    */
    ptr = plugin_call(return -1, item->plugin->item_ops.specific.ptr,
        get_ptr, item);
	    
    width = plugin_call(return -1, item->plugin->item_ops.specific.ptr,
        get_width, item);

    if (!ptr || (ptr >= reiser4_format_get_len(data->format)) || 
	(width >= reiser4_format_get_len(data->format)) || 
	(ptr + width >= reiser4_format_get_len(data->format))) 
	return 1;
	
    /* Check if no any formatted blocks exists after ptr. */
    if ((next_blk = aux_bitmap_find(repair_cut_data(data)->format_layout, ptr)) == 0)
        return 0;
    
    if (next_blk >= ptr && next_blk < ptr + width) 
	return 1;

    return 0;
}

errno_t repair_item_open(reiser4_item_t *item, reiser4_node_t *node, 
    reiser4_pos_t *pos)
{
    rpid_t pid;
    
    aal_assert("vpf-232", node != NULL, return -1);
    aal_assert("vpf-235", node->entity != NULL, return -1);
    aal_assert("vpf-236", node->entity->plugin != NULL, return -1);
    aal_assert("vpf-233", pos != NULL, return -1);
    aal_assert("vpf-234", item != NULL, return -1);
    
    /* Check items plugin ids. */
    /* FIXME-VITALY: There will be a fix for plugin ids in a future here. */
    pid = plugin_call(return 0, node->entity->plugin->node_ops, item_pid, 
	node->entity, pos);

    if ((pid == FAKE_PLUGIN) || !(item->plugin = 
	libreiser4_factory_ifind(ITEM_PLUGIN_TYPE, pid))) 
    {
        aal_exception_error("Node (%llu): unknown plugin is specified for "
	    "the item (%u).",aal_block_number(node->block), pos->item);
	return 1;
    }

    item->node = node->entity;
    item->pos = pos;

    return 0;
}

