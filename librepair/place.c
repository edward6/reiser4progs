/*
    librepair/place.c -- common place functions.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/librepair.h>

/* Just a useful method to handle position when an item or a unit is removed. */
void repair_place_left_pos_save(reiser4_place_t *current, rpos_t *pos) {
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

