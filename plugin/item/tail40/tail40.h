/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.h -- reiser4 tail plugin functions. */

#ifndef TAIL40_H
#define TAIL40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

#define tail40_body(place) (place->body)
extern uint32_t tail40_units(place_t *place);

extern errno_t tail40_maxreal_key(place_t *place,
				  key_entity_t *key);

extern errno_t tail40_copy(place_t *dst_place, uint32_t dst_pos,
			   place_t *src_place, uint32_t src_pos,
			   uint32_t count);
#endif
