/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.h -- reiser4 common tail functions. */

#ifndef TAIL40_H
#define TAIL40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

#define tail40_pos(place) ((place)->pos.unit + (place)->off)

extern uint32_t tail40_units(reiser4_place_t *place);

extern errno_t tail40_maxreal_key(reiser4_place_t *place,
				  reiser4_key_t *key);

extern errno_t tail40_maxposs_key(reiser4_place_t *place,
				  reiser4_key_t *key);

extern uint32_t tail40_expand(reiser4_place_t *place, uint32_t pos,
			      uint32_t count);

extern errno_t tail40_copy(reiser4_place_t *dst_place, uint32_t dst_pos,
			   reiser4_place_t *src_place, uint32_t src_pos,
			   uint32_t count);

extern errno_t tail40_prep_shift(reiser4_place_t *src_place,
				 reiser4_place_t *dst_place,
				 shift_hint_t *hint);

extern errno_t tail40_shift_units(reiser4_place_t *src_place,
			   reiser4_place_t *dst_place,
			   shift_hint_t *hint);

extern lookup_t tail40_lookup(reiser4_place_t *place,
			      lookup_hint_t *hint,
			      lookup_bias_t bias);

extern errno_t tail40_fetch_key(reiser4_place_t *place, 
				reiser4_key_t *key);

extern uint64_t tail40_size(reiser4_place_t *place);

extern errno_t tail40_prep_write(reiser4_place_t *place, 
				 trans_hint_t *hint);

extern int64_t tail40_write_units(reiser4_place_t *place, 
				  trans_hint_t *hint);

extern int64_t tail40_trunc_units(reiser4_place_t *place, 
				  trans_hint_t *hint);

extern int64_t tail40_read_units(reiser4_place_t *place,
				 trans_hint_t *hint);
#endif
