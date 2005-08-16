/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/item.h -- common structures and methods for item recovery. */

#ifndef REPAIR_ITEM_H
#define REPAIR_ITEM_H

#include <repair/repair.h>


extern errno_t repair_item_check_struct(reiser4_place_t *place,
					uint8_t mode);

extern errno_t repair_item_check_layout(reiser4_place_t *place, 
					region_func_t func, 
					void *data, uint8_t mode);

extern void repair_item_print(reiser4_place_t *place, aal_stream_t *stream);

#endif
