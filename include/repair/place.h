/*
    repair/place.h -- common structures and methods for place handling.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_COORD_H
#define REPAIR_COORD_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

#define repair_place_get_lpos(place, ppos)				\
{									\
    ppos = place->pos;							\
    if (reiser4_item_units(place) == 1 || place->pos.unit == ~0ul) {	\
	ppos.unit = ~0ul - 1;						\
	ppos.item--;							\
    } else {								\
	ppos.unit--;							\
    }									\
}

#endif

