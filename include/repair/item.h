/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/item.h -- common structures and methods for item recovery. */

#ifndef REPAIR_ITEM_H
#define REPAIR_ITEM_H

#include <repair/repair.h>

extern errno_t repair_item_check_struct(place_t *place,
					uint8_t mode);

extern errno_t repair_item_check_layout(place_t *place, 
					region_func_t func, 
					void *data, uint8_t mode);

extern errno_t repair_item_estimate_merge(place_t *dst, place_t *src, 
					  merge_hint_t *hint);

extern void repair_item_set_flag(place_t *place, uint16_t flag);
extern void repair_item_clear_flag(place_t *place, uint16_t flag);
extern bool_t repair_item_test_flag(place_t *place, uint16_t flag);
#endif
