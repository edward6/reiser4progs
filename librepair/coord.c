/*
    librepair/coord.c -- common repair item functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_item_fix_pointer(reiser4_coord_t *coord) {
    reiser4_ptr_hint_t hint;
    reiser4_pos_t prev;

    aal_assert("vpf-416", coord != NULL, return -1);
    aal_assert("vpf-417", reiser4_coord_node(coord) != NULL, return -1);
    
    if (plugin_call(return -1, coord->entity.plugin->item_ops,
	fetch, &coord->entity, coord->pos.unit, &hint, 1))
	return -1;
    
    if (hint.width == 1 && reiser4_item_extent(coord)) {
	/* For one unit extent pointer we can just zero the start block 
	 * number. */		    
		    
	aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
	    "(start %llu, count %llu) is zeroed.", 
	    aal_block_number(reiser4_coord_block(coord)), coord->pos.item, 
	    coord->pos.unit, hint.ptr, hint.width);

	hint.ptr = 0;
	hint.width = 1;

	if (plugin_call(return -1, coord->entity.plugin->item_ops,
	    update, &coord->entity, coord->pos.unit, &hint, 1))
	    return -1;	    
    } else {
	/* For many unit pointers there is no way to figure out what 
	 * is broken - the start block of the width. 
	 * Delete the unit if node pointer. 
	 * Delete the item if extent pointer. */
	
	if (reiser4_item_extent(coord))
	    coord->pos.unit = ~0ul;

	aal_exception_error("Node (%llu), item (%u), unit (%u): pointer "
	    "(start %llu, count %llu) is removed.", 
	    aal_block_number(reiser4_coord_block(coord)),
	    coord->pos.item, coord->pos.unit, hint.ptr, hint.width);

	repair_coord_left_pos_save(coord, &prev);
	if (reiser4_node_remove(reiser4_coord_node(coord), &coord->pos))
	    return -1;		    
		    
	coord->pos = prev;
    }

    return 0;
}

errno_t repair_item_ptr_bitmap_used(reiser4_coord_t *coord,
    aux_bitmap_t *bitmap, repair_check_t *data)
{
    blk_t next_blk;
    reiser4_ptr_hint_t ptr;
    int res;

    aal_assert("vpf-396", coord != NULL, return -1);
    aal_assert("vpf-397", data != NULL, return -1);
    aal_assert("vpf-272", reiser4_item_nodeptr(coord) || reiser4_item_extent(coord), 
	return -1);


    if (plugin_call(return -1, coord->entity.plugin->item_ops, fetch, 
	&coord->entity, coord->pos.unit, &ptr, 1))
	goto error;

    aal_assert("vpf-414", ptr.width, return -1);
    aal_assert("vpf-415", ptr.ptr || reiser4_item_extent(coord), return -1);

    if (!ptr.ptr)
	return 0;

    /* Check if no any unused block exists after ptr. */
    if ((next_blk = aux_bitmap_find(bitmap, ptr.ptr)) == 0)
	return 0;

    if (next_blk >= ptr.ptr && next_blk < ptr.ptr + ptr.width)
	goto error;

    return 0;
    
error:
    aal_exception_error("Node (%llu), item (%llu), unit (%llu): %s pointer "
	"(start %llu, count %llu) points some already used blocks.", 
	aal_block_number(reiser4_coord_block(coord)), coord->pos.item, 
	coord->pos.unit, reiser4_item_nodeptr(coord) ? "node" : "extent", 
	ptr.ptr, ptr.width);

    return 1;
}

errno_t repair_item_ptr_format_check(reiser4_coord_t *coord, 
    repair_check_t *data) 
{
    int res;
    blk_t next_blk;
    reiser4_ptr_hint_t ptr;

    aal_assert("vpf-270", coord != NULL, return -1);
    aal_assert("vpf-271", data != NULL, return -1);
    aal_assert("vpf-272", data->format != NULL, return -1);
    aal_assert("vpf-272", reiser4_item_nodeptr(coord) || reiser4_item_extent(coord), 
	return -1);

    if (plugin_call(return -1, coord->entity.plugin->item_ops, fetch,
	&coord->entity, coord->pos.unit, &ptr, 1))
	return -1;
	
    if ((!ptr.ptr && reiser4_item_nodeptr(coord)) || !ptr.width) 
	goto error;
    
    if ((ptr.ptr >= reiser4_format_get_len(data->format)) || 
	(ptr.width >= reiser4_format_get_len(data->format)) || 
	(ptr.ptr >= reiser4_format_get_len(data->format) - ptr.width)) 
	goto error;
    
    /* Check if no any formatted block exists after ptr. */
    if ((next_blk = aux_bitmap_find(repair_filter_data(data)->format_layout, 
	ptr.ptr)) == 0)
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

errno_t repair_coord_open(reiser4_coord_t *coord, void *data, 
    coord_context_t context, reiser4_pos_t *pos)
{
    /* FIXME-VITALY: There will be a fix for plugin ids in a future here. */
    
    return reiser4_coord_open(coord, data, context, pos);
}


void repair_coord_left_pos_save(reiser4_coord_t *current, reiser4_pos_t *prev) {
    *prev = current->pos;
    
    if ((current->pos.unit == 0 && reiser4_item_count(current) == 1) || 
	(current->pos.unit == ~0ul)) 
    {
	prev->item--;
	prev->unit = ~0ul;
    } else {	
	prev->unit--;
    }
}


