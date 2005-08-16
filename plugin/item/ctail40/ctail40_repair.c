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

#endif
