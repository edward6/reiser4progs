/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
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

struct extent40 {
	d64_t start;
	d64_t width;
};

typedef struct extent40 extent40_t;

extern reiser4_core_t *extent40_core;

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
