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

/*extern errno_t repair_item_ptr_used_in_format(reiser4_coord_t *coord,
    repair_data_t *data);*/
extern errno_t repair_item_ptr_used_in_bitmap(reiser4_coord_t *coord,
    aux_bitmap_t *bitmap, repair_data_t *data);
extern errno_t repair_item_handle_ptr(reiser4_coord_t *coord);

#endif

