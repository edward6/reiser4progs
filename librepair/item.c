/*
    librepair/item.c -- common repair item functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_item_handle_ptr(reiser4_coord_t *coord) {
    reiser4_ptr_hint_t hint;
    reiser4_pos_t prev;

    aal_assert("vpf-416", coord != NULL, return -1);
    aal_assert("vpf-417", coord->node != NULL, return -1);
    
    /* Fetch the pointer from the coord. */
    if (plugin_call(return -1, coord->item.plugin->item_ops,
	fetch, &coord->item, &hint, coord->pos.unit, 1) != 1)
	return -1;
    if (hint.width == 1 && reiser4_item_extent(coord)) {
	/* For one unit extent pointer we can just zero the start block 
	 * number. */
	aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
	    "(start %llu, count %llu) is zeroed.", 
	    coord->node->blk, coord->pos.item, 
	    coord->pos.unit, hint.ptr, hint.width);

	hint.ptr = 0;

	if (plugin_call(return -1, coord->item.plugin->item_ops,
	    update, &coord->item, &hint, coord->pos.unit, 1))
	    return -1;	    
    } else {
	/* For many unit pointers there is no way to figure out what 
	 * is broken - the start block of the width. 
	 * Delete the unit if node pointer. 
	 * Delete the item if extent pointer. */	

	/* Correct position to work with the whole item for extent items. */
	if (reiser4_item_extent(coord))
	    coord->pos.unit = ~0ul;

	aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
	    "(start %llu, count %llu) is removed.", 
	    coord->node->blk, coord->pos.item, coord->pos.unit,
	    hint.ptr, hint.width);

	repair_coord_left_pos_save(coord, &prev);
	if (reiser4_node_remove(coord->node, &coord->pos))
	    return -1;		    
		    
	coord->pos = prev;
    }

    return 0;
}

/* Blocks pointed by coord should not be used in bitmap. 
 * Returns -1 if fatal error; 1 if not used; 0 - ok. */
errno_t repair_item_ptr_unused(reiser4_coord_t *coord, aux_bitmap_t *bitmap) {
    blk_t next_blk;
    reiser4_ptr_hint_t ptr;
    int res;

    aal_assert("vpf-500", coord != NULL, return -1);
    aal_assert("vpf-397", bitmap != NULL, return -1);
    aal_assert("vpf-497", reiser4_item_nodeptr(coord) || 
	reiser4_item_extent(coord), return -1);

    if ((res = plugin_call(return -1, coord->item.plugin->item_ops, fetch, 
	&coord->item, &ptr, coord->pos.unit, 1)) != 1)
	return res;

    /* Ptr can be 0 if extent item only. Width cannot be 0. */
    if ((!ptr.ptr && reiser4_item_nodeptr(coord)) || !ptr.width) 
	goto error;

    if ((ptr.ptr >= bitmap->total) || (ptr.width >= bitmap->total) || 
	(ptr.ptr >= bitmap->total - ptr.width)) 
	goto error;
    
    if (!ptr.ptr)
	return 0;

    /* Check that ptr does not point any used block. */
    if (!aux_bitmap_test_range_cleared(bitmap, ptr.ptr, ptr.ptr + ptr.width))
	goto error;
	
    return 0;
    
error:
    aal_exception_error("Node (%llu), item (%u), unit (%u): %s pointer "
	"(start %llu, count %llu) points to some already used blocks.", 
	coord->node->blk, coord->pos.item, 
	coord->pos.unit, reiser4_item_nodeptr(coord) ? "node" : "extent", 
	ptr.ptr, ptr.width);

    return 1;
}

/* Prepare coord for just a piece of item insertion. Return the number of 
 * units to be splited. */
uint32_t repair_item_split(
    reiser4_coord_t *coord, 
    reiser4_key_t *rd_key)  /* split to this right delimiting key */
{    
    reiser4_key_t key;
    uint32_t unit = 0;

    if (reiser4_item_max_real_key(coord, &key))
	return -1;
 
    /* rd_key greater then max real key - nothing to split. */
    if (reiser4_key_compare(rd_key, &key) > 0) 
	return reiser4_item_units(coord);
 
    /* Split. Lookup method must be implemented in this case. */
    if (plugin_call(return -1, coord->item.plugin->item_ops, lookup, 
	&coord->item, rd_key, &unit) == -1) 
    {
	aal_exception_error("Lookup in the item %d in the node %llu failed.", 
	    coord->pos.item, coord->node->blk);
	return -1;
    }       
   
    return unit;
}

/* Blocks pointed by coord should not be used in bitmap. 
 * Returns -1 if fatal error; 1 if not used; 0 - ok. */
/*
errno_t repair_item_ptr_used_in_format(reiser4_coord_t *coord, 
    repair_data_t *data) 
{
    int res;
    blk_t next_blk;
    reiser4_ptr_hint_t ptr;

    aal_assert("vpf-270", coord != NULL, return -1);
    aal_assert("vpf-271", data != NULL, return -1);
    aal_assert("vpf-272", data->format != NULL, return -1);
    aal_assert("vpf-496", reiser4_item_nodeptr(coord) || reiser4_item_extent(coord), 
	return -1);

    if (plugin_call(return -1, coord->item.plugin->item_ops, fetch,
	&coord->item, coord->pos.unit, &ptr, 1))
	return -1;
	
    if ((!ptr.ptr && reiser4_item_nodeptr(coord)) || !ptr.width) 
	goto error;
    
    if ((ptr.ptr >= reiser4_format_get_len(data->format)) || 
	(ptr.width >= reiser4_format_get_len(data->format)) || 
	(ptr.ptr >= reiser4_format_get_len(data->format) - ptr.width)) 
	goto error;
    
    // Check if no any formatted block exists after ptr. 
    // FIXME-VITALY: should not depend on filter specific data. 
    if ((next_blk = aux_bitmap_find_marked(
	repair_filter_data(data)->bm_format, ptr.ptr)) == INVAL_BLK)
        return 0;
    
    if (next_blk >= ptr.ptr && next_blk < ptr.ptr + ptr.width) 
	return 1;

    return 0;
    
error:
    aal_exception_error("Node (%llu), item (%u), unit(%u): %s pointer "
	"(start (%llu), count (%llu)) points some reiser4 system blocks.", 
	aal_block_number(reiser4_coord_block(coord)), coord->pos.item, 
	coord->pos.unit, reiser4_item_nodeptr(coord) ? "node" : "extent",
	ptr.ptr, ptr.width);
	
    return 1;
}
*/
