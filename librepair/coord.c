/*
    librepair/coord.c -- common coord functions.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#include <repair/librepair.h>

void repair_coord_left_pos_save(reiser4_coord_t *current, reiser4_pos_t *pos) {
    *pos = current->pos;
    
    if ((current->pos.unit == 0 && reiser4_item_units(current) == 1) || 
	(current->pos.unit == ~0ul)) 
    {
	pos->item--;
	pos->unit = ~0ul;
    } else {	
	pos->unit--;
    }
}

