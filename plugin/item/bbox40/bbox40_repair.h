/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   bbox40_repair.h -- black box, reiser4 default safe link plugin repair method
   declarations. */

#ifndef BLACKBOX40_REPAIR_H
#define BLACKBOX40_REPAIR_H

#ifndef ENABLE_MINIMAL
#include <reiser4/plugin.h>

extern reiser4_core_t *bbox40_core;

extern errno_t bbox40_prep_insert_raw(reiser4_place_t *place,
				      trans_hint_t *hint);

extern errno_t bbox40_insert_raw(reiser4_place_t *place,
				 trans_hint_t *hint);

extern errno_t bbox40_check_struct(reiser4_place_t *place, 
				   repair_hint_t *hint);

extern void bbox40_print(reiser4_place_t *place, 
			 aal_stream_t *stream,
			 uint16_t options);

#endif
#endif
