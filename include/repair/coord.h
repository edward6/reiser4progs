/*
    repair/coord.h -- common structures and methods for coord handling.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef REPAIR_COORD_H
#define REPAIR_COORD_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_coord_open(reiser4_coord_t *coord, void *data,
    coord_context_t context, reiser4_pos_t *pos);
extern void repair_coord_left_pos_save(reiser4_coord_t *coord, 
    reiser4_pos_t *pos);

#endif

