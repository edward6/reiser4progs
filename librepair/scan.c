/*
    librepair/scan.c - methods are needed for the fsck pass2. 
    Copyright (C) 1996-2002 Hans Reiser.

    The second fsck pass - scan - fsck zeros extent pointers which point 
    to an already used block. Builds a map of used blocks.
*/

#include <repair/librepair.h>

/* 
    Zero extent pointers which point to an already used block. 
    Returns -1 if block is used already.
*/
static errno_t repair_node_handle_pointers(reiser4_node_t *node, 
    repair_check_t *data) 
{
    reiser4_coord_t coord;
    reiser4_pos_t pos = {0, 0};
    
    aal_assert("vpf-384", node != NULL, return -1);
    aal_assert("vpf-385", data != NULL, return -1);
    aal_assert("vpf-386", !aux_bitmap_test(repair_scan_data(data)->used, 
	aal_block_number(node->block)), return -1);

    for (pos.item = 0; pos.item < reiser4_node_count(node); pos.item++)  {	
	if (repair_coord_open(&coord, node, CT_NODE, &pos)) {
	    aal_exception_error("Node (%llu): failed to open the item (%u).", 
		aal_block_number(node->block), pos.item);
	    return -1;
	}	    

	if (!reiser4_item_extent(&coord) && !reiser4_item_nodeptr(&coord))
	    continue;

	for (pos.unit = 0; pos.unit < reiser4_item_count(&coord); pos.unit++) {
		reiser4_ptr_hint_t ptr;
	    blk_t form_blk, used_blk;

	    if (plugin_call(return -1, coord.entity.plugin->item_ops, fetch,
		    &coord.entity, 0, &ptr, 1))
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

errno_t repair_scan_node_check(reiser4_joint_t *joint, void *data) {
    return repair_node_handle_pointers(joint->node, (repair_check_t *)data);
}


/* Reinitialize data and setup data->pass.scan. */
errno_t repair_scan_setup(reiser4_fs_t *fs, repair_check_t *data) {
    /* data->pass.scan.(format_layout|used) are initialized from 
     * data->pass.filter.(format_layout|formatted) due to data->pass 
     * unit structure. */

    return 0;
}
