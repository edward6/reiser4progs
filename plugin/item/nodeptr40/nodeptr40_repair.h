/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40_repair.h -- reiser4 nodeptr repair functions. */

#ifndef NODEPTR40_REPAIR_H
#define NODEPTR40_REPAIR_H

#ifndef ENABLE_MINIMAL
#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern void nodeptr40_print(reiser4_place_t *place,
			    aal_stream_t *stream,
			    uint16_t options);

extern errno_t nodeptr40_check_struct(reiser4_place_t *place,
				      repair_hint_t *hint);

extern errno_t nodeptr40_check_layout(reiser4_place_t *place,
				      repair_hint_t *hint, 
				      region_func_t func, 
				      void *data);
#endif
#endif
