/*
    repair/item.h -- common structures and methods for item recovery.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef REPAIR_ITEM_H
#define REPAIR_ITEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern uint32_t repair_item_split(reiser4_coord_t *coord, 
    reiser4_key_t *rd_key);

#endif

