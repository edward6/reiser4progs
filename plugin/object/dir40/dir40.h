/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.h -- reiser4 hashed directory plugin structures. */

#ifndef DIR40_H
#define DIR40_H

#include <aal/aal.h>
#include <sys/stat.h>
#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

/* Compaund directory structure */
struct dir40 {
	
	/* Common fields for all files (statdata, etc) */
	obj40_t obj;

	/* Current body item coord */
	place_t body;
	
	/* Current position in the directory (key and adjust). Adjust is needed
	   to work fine when key collitions take place. */
#ifndef ENABLE_STAND_ALONE
	uint32_t adjust;
#endif
	
	key_entity_t offset;

	/* Hash plugin in use */
	reiser4_plug_t *hash;
};

typedef struct dir40 dir40_t;

extern reiser4_plug_t dir40_plug;
extern reiser4_core_t *dir40_core;

extern errno_t dir40_reset(object_entity_t *entity);
extern lookup_t dir40_next(dir40_t *dir, bool_t check);
extern int32_t dir40_belong(dir40_t *dir, place_t *place);

extern lookup_t dir40_lookup(object_entity_t *entity,
			     char *name, entry_hint_t *entry);

extern errno_t dir40_fetch(dir40_t *dir, entry_hint_t *entry);
#endif
