/*
    librepair/coord.c -- common repair item functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_coord_ptr_check(reiser4_coord_t *coord, repair_check_t *data) {
    int res;
    blk_t next_blk;
    reiser4_ptr_hint_t ptr;

    aal_assert("vpf-270", coord != NULL, return -1);
    aal_assert("vpf-271", data != NULL, return -1);
    aal_assert("vpf-272", data->format != NULL, return -1);

    /*
	FIXME-VITALY: This stuff should be tested carefully when functions 
	return 0 as an error. 
    */
    if (plugin_call(return -1, coord->entity.plugin->item_ops, fetch,
	    &coord->entity, 0, &ptr, 1))
	return -1;
	
    if (!ptr.ptr || (ptr.ptr >= reiser4_format_get_len(data->format)) || 
	(ptr.width >= reiser4_format_get_len(data->format)) || 
	(ptr.ptr + ptr.width >= reiser4_format_get_len(data->format))) 
	return 1;
	
    /* Check if no any formatted blocks exists after ptr. */
    if ((next_blk = aux_bitmap_find(repair_filter_data(data)->format_layout, 
	ptr.ptr)) == 0)
    {
        return 0;
    }
    
    if (next_blk >= ptr.ptr && next_blk < ptr.ptr + ptr.width) 
	return 1;

    return 0;
}

errno_t repair_coord_open(reiser4_coord_t *coord, void *data, 
    coord_context_t context, reiser4_pos_t *pos)
{
    /* FIXME-VITALY: There will be a fix for plugin ids in a future here. */
    
    return reiser4_coord_open(coord, data, context, pos);
}

