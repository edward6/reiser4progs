/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40_repair.h -- reiser4 stat data repair functions. */

#ifndef STAT40_REPAIR_H
#define STAT40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t stat40_check_struct(place_t *place,
				   uint8_t mode);

extern errno_t stat40_merge(place_t *dst, place_t *src, 
			    merge_hint_t *hint);

extern errno_t stat40_estimate_merge(place_t *dst, place_t *src, 
				     merge_hint_t *hint);
#endif
