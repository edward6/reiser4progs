/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plain40_repair.c -- reiser4 plain tail item plugin repair functions. */

#ifndef ENABLE_MINIMAL

#include "plain40.h"
#include <plugin/item/tail40/tail40_repair.h>

errno_t plain40_prep_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	place->off = 0;
	return tail40_prep_insert_raw(place, hint);
}

#endif
