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

extern errno_t repair_item_ptr_format_check(reiser4_coord_t *coord,
    repair_check_t *data);
extern errno_t repair_item_ptr_bitmap_used(reiser4_coord_t *coord,
    aux_bitmap_t *bitmap, repair_check_t *data);
extern errno_t repair_item_fix_pointer(reiser4_coord_t *coord);

#endif

