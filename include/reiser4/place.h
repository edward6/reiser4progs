/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   place.h -- reiser4 tree place functions. */

#ifndef REISER4_PLACE_H
#define REISER4_PLACE_H

#include <reiser4/types.h>

extern bool_t reiser4_place_valid(place_t *place);
extern errno_t reiser4_place_last(place_t *place);
extern errno_t reiser4_place_first(place_t *place);
extern errno_t reiser4_place_fetch(place_t *place);
extern bool_t reiser4_place_ltlast(place_t *place);
extern bool_t reiser4_place_gtfirst(place_t *place);

#ifndef ENABLE_STAND_ALONE
extern bool_t reiser4_place_right(place_t *place);
extern bool_t reiser4_place_leftmost(place_t *place);
extern bool_t reiser4_place_rightmost(place_t *place);
extern void reiser4_place_inc(place_t *place, int whole);
extern void reiser4_place_dec(place_t *place, int whole);
#endif

extern errno_t reiser4_place_assign(place_t *place, node_t *node,
				    uint32_t item, uint32_t unit);

extern errno_t reiser4_place_init(place_t *place, node_t *node,
				  pos_t *pos);

extern errno_t reiser4_place_open(place_t *place, node_t *node,
				  pos_t *pos);
#endif
