/*
    librepair/item.c -- common repair item functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_item_nptr_check(reiser4_node_t *node, 
    reiser4_item_t *item, repair_check_t *data) 
{
    blk_t blk, next_blk;
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
    blk = reiser4_item_get_nptr(item);
    width = reiser4_item_get_nwidth(item);

    if (!blk || (blk >= reiser4_format_get_len(data->format)) || 
	(width >= reiser4_format_get_len(data->format)) || 
	(blk + width >= reiser4_format_get_len(data->format))) 
    {
	if (reiser4_item_internal(item)) {
	    if (reiser4_node_remove(node, item->pos))
		return -1;
	} else if (reiser4_item_extent(item)) {
	    reiser4_item_set_nptr(item, 0);
	}
	
	return 0;
    }
	
    if ((next_blk = aux_bitmap_find(repair_cut_data(data)->format_layout, 
	blk)) == 0)
	/* No any formatted blocks exists after blk. */
        return 0;
    
    if (next_blk >= blk && next_blk < blk + width) {
	if (reiser4_item_internal(item)) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): bad "
		"internal pointer (%llu). Removed.", aal_block_number(node->block), 
		item->pos->item, item->pos->unit, blk);
	    if (reiser4_node_remove(node, item->pos))
		return -1;
	} else if (reiser4_item_extent(item)) {
	    aal_exception_error("Node (%llu), item (%u), unit (%u): bad extent "
		"pointer (%llu). Zeroed.", aal_block_number(node->block), blk);
		    
	    reiser4_item_set_nptr(item, 0);
	}
    }

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

