/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   place.h -- reiser4 tree place functions. */

#ifndef REISER4_PLACE_H
#define REISER4_PLACE_H

#include <reiser4/types.h>

extern void reiser4_place_inc(reiser4_place_t *place,
			      int whole);

extern void reiser4_place_dec(reiser4_place_t *place,
			      int whole);

#ifndef ENABLE_STAND_ALONE
extern bool_t reiser4_place_rightmost(reiser4_place_t *place);
extern bool_t reiser4_place_leftmost(reiser4_place_t *place);
#endif

extern bool_t reiser4_place_valid(reiser4_place_t *place);
extern bool_t reiser4_place_gtfirst(reiser4_place_t *place);
extern bool_t reiser4_place_ltlast(reiser4_place_t *place);
extern errno_t reiser4_place_first(reiser4_place_t *place);
extern errno_t reiser4_place_last(reiser4_place_t *place);
extern errno_t reiser4_place_fetch(reiser4_place_t *place);

extern errno_t reiser4_place_assign(reiser4_place_t *place,
				    reiser4_node_t *node,
				    uint32_t item,
				    uint32_t unit);

extern errno_t reiser4_place_init(reiser4_place_t *place,
				  reiser4_node_t *node,
				  pos_t *pos);

extern errno_t reiser4_place_open(reiser4_place_t *place,
				  reiser4_node_t *node,
				  pos_t *pos);

#endif
