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

extern void repair_place_left_pos_save(reiser4_place_t *place, 
    rpos_t *pos);

#endif

