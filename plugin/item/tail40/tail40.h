/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.h -- reiser4 tail plugin functions. */

#ifndef TAIL40_H
#define TAIL40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

#define tail40_body(place) (place->body)

extern reiser4_core_t *tail40_core;

extern uint32_t tail40_units(reiser4_place_t *place);

extern errno_t tail40_maxreal_key(reiser4_place_t *place,
				  reiser4_key_t *key);

extern uint32_t tail40_expand(reiser4_place_t *place, uint32_t pos,
			      uint32_t count);

extern errno_t tail40_copy(reiser4_place_t *dst_place, uint32_t dst_pos,
			   reiser4_place_t *src_place, uint32_t src_pos,
			   uint32_t count);
#endif
