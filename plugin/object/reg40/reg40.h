/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.h -- reiser4 regular file plugin structures. */

#ifndef REG40_H
#define REG40_H

#include <aal/aal.h>
#include <sys/stat.h>
#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

/* Regular file structure */
struct reg40 {

	/* Common fields (statdata, etc) */
	obj40_t obj;

	/* Current body item coord stored here */
	place_t body;

	/* Current position in the reg file */
	key_entity_t offset;

#ifndef ENABLE_STAND_ALONE
	/* Tail policy plugin */
	reiser4_plug_t *policy;
#endif
};

typedef struct reg40 reg40_t;
#endif

