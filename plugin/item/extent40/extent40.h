/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40.h -- reiser4 default extent plugin. */

#ifndef EXTENT40_H
#define EXTENT40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

typedef enum insert_flag {
	/* Add some units at the end. */
	ET40_OVERWRITE	= 1 << 0,
	/* There is a head left in the current dst unit while overwriting. */
	ET40_HEAD	= 1 << 1,
	/* There is a tail left in the current dst unit while overwriting. */
	ET40_TAIL	= 1 << 2,
	/* Join new unit and existent one. */
	ET40_JOIN	= 1 << 3
} insert_flag_t;

typedef struct extent40 {
	d64_t start;
	d64_t width;
} extent40_t;

extern reiser4_core_t *extent40_core;

extern int extent40_mergeable(reiser4_place_t *place1,
			      reiser4_place_t *place2);

extern errno_t extent40_prep_shift(reiser4_place_t *src_place,
				   reiser4_place_t *dst_place,
				   shift_hint_t *hint);

extern errno_t extent40_shift_units(reiser4_place_t *src_place,
				    reiser4_place_t *dst_place,
				    shift_hint_t *hint);

extern errno_t extent40_fetch_key(reiser4_place_t *place, reiser4_key_t *key);

extern errno_t extent40_maxposs_key(reiser4_place_t *place,
				    reiser4_key_t *key);
extern errno_t extent40_remove_units(reiser4_place_t *place,
				     trans_hint_t *hint);
extern int64_t extent40_update_units(reiser4_place_t *place,
				     trans_hint_t *hint);
extern errno_t extent40_prep_insert(reiser4_place_t *place, trans_hint_t *hint);
extern int64_t extent40_insert_units(reiser4_place_t *place,
				     trans_hint_t *hint);
extern errno_t extent40_prep_write(reiser4_place_t *place, trans_hint_t *hint);
extern int64_t extent40_write_units(reiser4_place_t *place, trans_hint_t *hint);
extern int64_t extent40_trunc_units(reiser4_place_t *place, trans_hint_t *hint);
extern errno_t extent40_layout(reiser4_place_t *place,
			       region_func_t region_func,
			       void *data);
extern uint64_t extent40_size(reiser4_place_t *place);
extern uint64_t extent40_bytes(reiser4_place_t *place);
extern int64_t extent40_read_units(reiser4_place_t *place, trans_hint_t *hint);
extern int64_t extent40_fetch_units(reiser4_place_t *place, trans_hint_t *hint);
extern uint32_t extent40_units(reiser4_place_t *place);
extern uint64_t extent40_offset(reiser4_place_t *place,
				uint32_t pos);
extern uint32_t extent40_unit(reiser4_place_t *place,
			      uint64_t offset);
extern lookup_t extent40_lookup(reiser4_place_t *place,
				lookup_hint_t *hint,
				lookup_bias_t bias);
extern errno_t extent40_maxreal_key(reiser4_place_t *place,
				    reiser4_key_t *key);
extern uint32_t extent40_expand(reiser4_place_t *place,
				uint32_t pos, uint32_t count);
extern uint32_t extent40_shrink(reiser4_place_t *place,
				uint32_t pos, uint32_t count);

#define extent40_device(place)  \
        ((place)->node->block->device)

#define extent40_secsize(place) \
        (extent40_device(place)->blksize)

#define extent40_body(place)	\
        ((extent40_t *)(place)->body)

#define et40_get_start(et)	\
        aal_get_le64((et), start)

#define et40_set_start(et, val)	\
        aal_set_le64((et), start, val)

#define et40_get_width(et)	\
        aal_get_le64((et), width)

#define et40_set_width(et, val)	\
        aal_set_le64((et), width, val)

#define et40_inc_start(et, val) \
        et40_set_start((et), et40_get_start((et)) + val)

#define et40_dec_start(et, val) \
        et40_set_start((et), et40_get_start((et)) - val)

#define et40_inc_width(et, val) \
        et40_set_width((et), et40_get_width((et)) + val)

#define et40_dec_width(et, val) \
        et40_set_width((et), et40_get_width((et)) - val)

#endif
