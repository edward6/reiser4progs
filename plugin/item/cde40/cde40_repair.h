/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40_repair.h -- reiser4 directory entry plugin repair functions. */

#ifndef CDE40_REPAIR_H
#define CDE40_REPAIR_H

#ifndef ENABLE_MINIMAL
#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t cde40_prep_insert_raw(reiser4_place_t *place,
				     trans_hint_t *hint);

extern errno_t cde40_insert_raw(reiser4_place_t *place,
				trans_hint_t *hint);

extern errno_t cde40_check_struct(reiser4_place_t *place,
				  repair_hint_t *hint);

extern void cde40_print(reiser4_place_t *place,
			aal_stream_t *stream,
			uint16_t options);
#endif
#endif
