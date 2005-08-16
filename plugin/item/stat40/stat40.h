/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40.h -- reiser4 default stat data structures. */

#ifndef STAT40_H
#define STAT40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

/* Macro for getting stat data body from item */
#define stat40_body(place) ((stat40_t *)place->body)

/* Type for stat40 layout callback function */
typedef errno_t (*ext_func_t) (stat_entity_t *,
			       uint64_t, void *);

extern errno_t stat40_traverse(reiser4_place_t *place,
			       ext_func_t ext_func,
			       void *data);

typedef struct stat40 {
	d16_t extmask;
} stat40_t;

extern reiser4_core_t *stat40_core;

#define STAT40_EXTNR (64)

#define st40_get_extmask(stat)		aal_get_le16(((stat40_t *)stat), extmask)
#define st40_set_extmask(stat, val)	aal_set_le16(((stat40_t *)stat), extmask, val)

#endif
