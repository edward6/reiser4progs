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

extern errno_t repair_item_nptr_check(reiser4_node_t *node, 
    reiser4_coord_t *coord, repair_check_t *data);

extern errno_t repair_item_open(reiser4_coord_t *coord, reiser4_node_t *node, 
    reiser4_pos_t *pos);

#endif

