/*
    librepair/coord.c -- common coord functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

errno_t repair_coord_open(reiser4_coord_t *coord, void *data, 
    coord_context_t context, reiser4_pos_t *pos)
{
    /* FIXME-VITALY: There will be a fix for plugin ids in a future here. */
    
    return reiser4_coord_open(coord, data, context, pos);
}


void repair_coord_left_pos_save(reiser4_coord_t *current, reiser4_pos_t *pos) {
    *pos = current->pos;
    
    if ((current->pos.unit == 0 && reiser4_item_count(current) == 1) || 
	(current->pos.unit == ~0ul)) 
    {
	pos->item--;
	pos->unit = ~0ul;
    } else {	
	pos->unit--;
    }
}

