/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40_repair.h -- reiser4 stat data repair functions. */

#ifndef STAT40_REPAIR_H
#define STAT40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t stat40_prep_insert_raw(reiser4_place_t *place, 
				      trans_hint_t *hint);

extern errno_t stat40_insert_raw(reiser4_place_t *place, 
				 trans_hint_t *hint);

extern errno_t stat40_check_struct(reiser4_place_t *place,
				   repair_hint_t *hint);

extern void stat40_print(reiser4_place_t *place, 
			 aal_stream_t *stream, 
			 uint16_t options);

#endif
