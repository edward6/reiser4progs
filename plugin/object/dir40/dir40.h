/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.h -- reiser4 hashed directory plugin structures. */

#ifndef DIR40_H
#define DIR40_H

#include <aal/libaal.h>
#include "plugin/object/obj40/obj40.h"

/* Compaund directory structure */
struct dir40 {
	/* Common fields for all files (statdata, etc) */
	obj40_t obj;

	/* Current body item coord */
	reiser4_place_t body;

	/* Current directory offset. */
	reiser4_key_t position;
};

typedef struct dir40 dir40_t;

extern reiser4_plug_t dir40_plug;
extern reiser4_core_t *dir40_core;

extern lookup_t dir40_next(dir40_t *dir, int first);
extern errno_t dir40_reset(object_entity_t *entity);

extern lookup_t dir40_lookup(object_entity_t *entity,
			     char *name, entry_hint_t *entry);

extern errno_t dir40_fetch(dir40_t *dir, entry_hint_t *entry);

extern lookup_t dir40_update_body(object_entity_t *entity, int check_group);

#endif
