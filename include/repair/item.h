/*
    repair/item.h -- common structures and methods for item recovery.

    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_ITEM_H
#define REPAIR_ITEM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_item_check_struct(reiser4_place_t *place, uint8_t mode);

extern errno_t repair_item_check_layout(reiser4_place_t *place, 
    region_func_t func, void *data, uint8_t mode);

extern errno_t repair_item_estimate_copy(reiser4_place_t *dst, 
    reiser4_place_t *src, copy_hint_t *hint);

#endif

