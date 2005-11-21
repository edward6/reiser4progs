/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ctail40_repair.h -- reiser4 compressed file body item repair declarations. */

#ifndef CTAIL40_REPAIR_H
#define CTAIL40_REPAIR_H

extern errno_t ctail40_prep_insert_raw(reiser4_place_t *place, 
				       trans_hint_t *hint);

extern void ctail40_print(reiser4_place_t *place, 
			  aal_stream_t *stream, 
			  uint16_t options);
#endif
