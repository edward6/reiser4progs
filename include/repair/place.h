/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/place.h -- common structures and methods for place handling. */

#ifndef REPAIR_COORD_H
#define REPAIR_COORD_H

#include <repair/repair.h>

#define repair_place_get_lpos(place, ppos)					\
{										\
	ppos = place->pos;							\
										\
	if (reiser4_item_units(place) == 1 || place->pos.unit == MAX_UINT32) {	\
		ppos.unit = MAX_UINT32 - 1;					\
		ppos.item--;							\
	} else {								\
		ppos.unit--;							\
	}									\
}

#endif
