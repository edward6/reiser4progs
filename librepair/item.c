/*
    librepair/item.c - methods are needed for item recovery.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Vitaly Fertman.
*/

#include <repair/librepair.h>
#include <reiser4/reiser4.h>

errno_t repair_item_nptr_check(reiser4_item_t *item, repair_check_t *data) {
    blk_t blk;
    int res;

    aal_assert("vpf-270", item != NULL, return -1);
    aal_assert("vpf-271", data != NULL, return -1);
    aal_assert("vpf-272", data->format != NULL, return -1);
        
    if ((blk = reiser4_item_get_nptr(item))) {
	if ((res = reiser4_format_layout(data->format, callback_data_block_check, 
	    &blk)) && res != -1) 
	{
	    if (reiser4_item_internal(item)) {
		aal_exception_error("Node (llu), item (%u), unit (%u): bad internal "
		    "pointer (%llu). Removed.", item->pos->item, 
		    item->pos->unit, blk);

		if (item->pos->unit == ~0ul) {
		    if (plugin_call(return -1, item->node->plugin->node_ops, 
			remove, item->node, item->pos))
			return -1;
		    item->pos->item--;
		} else {
		    if (plugin_call(return -1, item->node->plugin->node_ops, 
			cut, item->node, item->pos))
			return -1;			    
		    item->pos->unit--;
		}
	    } else if (reiser4_item_extent(item)) {
		aal_exception_error("Node llu, item %u, unit %u: bad extent "
		    "pointer (%llu). Zeroed.", blk);
		reiser4_item_set_nptr(item, 0);
		/* 
		    FIXME-VITALY: extent points many blocks, all of them should be 
		    checked. 
		*/
	    }
		
	    return 0;
	}
    } 
    
    return -1;
}
