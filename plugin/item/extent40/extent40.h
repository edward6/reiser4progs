/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   extent40 -- resier4 default extent plugin. */

#ifndef EXTENT40
#define EXTENT40

#include <aal/aal.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

struct extent40 {
	d64_t start;
	d64_t width;
};

typedef struct extent40 extent40_t;

#define extent40_blksize(place) \
        ((place)->block->size)

#define extent40_device(place)  \
        ((place)->block->device)

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
