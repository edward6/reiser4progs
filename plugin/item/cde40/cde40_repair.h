/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40_repair.h -- reiser4 directory entry plugin repair functions. */

#ifndef CDE40_REPAIR_H
#define CDE40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t cde40_prep_merge(place_t *dst, place_t *src,
			       merge_hint_t *hint);

extern errno_t cde40_merge_units(place_t *dst, place_t *src, 
				 merge_hint_t *hint);

extern errno_t cde40_check_struct(place_t *place, uint8_t mode);
#endif
