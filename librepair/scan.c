/*
    librepair/scan.c - methods are needed for the fsck pass2. 
    Copyright (C) 1996-2002 Hans Reiser.

    The second fsck pass - scan - fsck zeros extent pointers which point 
    to an already used block. Builds a map of used blocks.
*/

#include <repair/librepair.h>

/* To be removed when relable. */
errno_t repair_scan_joint_check(reiser4_joint_t *joint, traverse_hint_t *hint) {
    repair_data_t *repair_data = (repair_data_t *)hint->data;

    aal_assert("vpf-427", repair_data != NULL, return -1);
    aal_assert("vpf-428", joint != NULL, return -1);
    aal_assert("vpf-429", joint->node != NULL, return -1);

    aal_assert("vpf-426", aux_bitmap_test(repair_scan_data(repair_data)->bm_used, 
	joint->node->blk), return -1);

    return 0;    
}

/* 
    Zero extent pointers which point to an already used block. 
    Returns -1 if block is used already.
*/
errno_t repair_scan_handle_pointers(reiser4_coord_t *coord, traverse_hint_t *hint) {
    repair_data_t *repair_data = (repair_data_t *)hint->data;
    reiser4_ptr_hint_t ptr;
    int res;
 
    aal_assert("vpf-384", coord != NULL, return -1);
    aal_assert("vpf-385", repair_data != NULL, return -1);

    aal_assert("vpf-386", 
	aux_bitmap_test(repair_scan_data(repair_data)->bm_used, 
	    reiser4_coord_blk(coord)), return -1);

    if (plugin_call(return -1, coord->entity.plugin->item_ops,
	fetch, &coord->entity, coord->pos.unit, &ptr, 1))
	return -1;
 
    /* This must be fixed at the first pass. */
    aal_assert("vpf-387", 
	(ptr.ptr < reiser4_format_get_len(repair_data->format)) && 
	(ptr.width < reiser4_format_get_len(repair_data->format)) && 
	(ptr.ptr < reiser4_format_get_len(repair_data->format) - ptr.width), 
	return -1);

    /* FIXME-VITALY: Improve it later - it could be just width to be
     * obviously wrong. Or start block. Give a hint into 
     * repair_item_ptr_used_in_format which returns what is obviously 
     * wrong. */

    if ((res = repair_item_ptr_used_in_bitmap(coord, 
	repair_scan_data(repair_data)->bm_used, repair_data)) < 0) 
    {
	return res;
    } else if (res > 0) {
	if (repair_item_handle_ptr(coord))
	    return -1;
    } else {
	aux_bitmap_mark_range(repair_scan_data(repair_data)->bm_used, ptr.ptr, 
	    ptr.ptr + ptr.width);
    }

    return 0;
}

errno_t repair_scan_node_check(reiser4_joint_t *joint, void *data) {
    traverse_hint_t hint;

    aal_assert("vpf-498", joint != NULL, return -1);
    aal_assert("vpf-499", data != NULL, return -1);
    
    hint.objects = 1 << EXTENT_ITEM;
    hint.data = data;
    hint.cleanup = 1;
    
    return reiser4_joint_traverse(joint, &hint, NULL, repair_scan_joint_check, 
	repair_scan_handle_pointers, NULL, NULL);
}

