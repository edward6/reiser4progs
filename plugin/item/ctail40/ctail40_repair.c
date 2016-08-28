/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ctail40_repair.c -- reiser4 compressed item file body plugin. */

#ifndef ENABLE_MINIMAL
#include "ctail40.h"
#include <plugin/item/tail40/tail40_repair.h>

errno_t ctail40_prep_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	place->off = sizeof(ctail40_t);
	return tail40_prep_insert_raw(place, hint);
}

void ctail40_print(reiser4_place_t *place, 
		   aal_stream_t *stream, 
		   uint16_t options)
{
	aal_assert("vpf-1892", place != NULL);
	aal_assert("vpf-1893", stream != NULL);
	
	aal_stream_format(stream, " shift=%lu\n", 
			  ct40_get_shift(place->body));
}

#endif
