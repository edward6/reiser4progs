/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40_repair.h -- reiser4 extent plugin repair functions. */

#ifndef EXTENT40_REPAIR_H
#define EXTENT40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern void extent40_print(reiser4_place_t *place,
			   aal_stream_t *stream, 
			   uint16_t options);

extern errno_t extent40_merge(reiser4_place_t *place,
			      trans_hint_t *hint);

extern errno_t extent40_check_struct(reiser4_place_t *place,
				     repair_hint_t *hint);

extern errno_t extent40_prep_merge(reiser4_place_t *place,
				   trans_hint_t *hint);

extern errno_t extent40_check_layout(reiser4_place_t *place, 
 				     repair_hint_t *hint, 
				     region_func_t region_func,
				     void *data);

#endif
