/*
    repair/coord.h -- common structures and methods for item recovery.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef REPAIR_COORD_H
#define REPAIR_COORD_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_coord_ptr_check(reiser4_coord_t *coord, repair_check_t *data);
extern errno_t repair_coord_open(reiser4_coord_t *coord, void *data,
    coord_context_t context, reiser4_pos_t *pos);

#endif

