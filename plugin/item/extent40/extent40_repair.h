/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40_repair.h -- reiser4 extent plugin repair functions. */

#ifndef EXTENT40_REPAIR_H
#define EXTENT40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t extent40_check_struct(place_t *place,
				     uint8_t mode);

extern errno_t extent40_merge(place_t *dst, place_t *src, 
			      merge_hint_t *hint);

extern errno_t extent40_estimate_merge(place_t *dst, place_t *src,
				       merge_hint_t *hint);

extern errno_t extent40_check_layout(place_t *place,
				     region_func_t region_func, 
				     void *data, uint8_t mode);
#endif
