/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.h -- reiser4 hashed directory plugin structures. */

#ifndef DIR40_H
#define DIR40_H

#include <aal/aal.h>
#include <sys/stat.h>
#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

static reiser4_core_t *core = NULL;

/* Compaund directory structure */
struct dir40 {
	
	/* Common fields for all files (statdata, etc) */
	obj40_t obj;

	/* Current body item coord */
	place_t body;
	
	/* Current position in the directory (key and adjust). Adjust is needed
	   to work fine when key collitions take place. */
#ifdef ENABLE_COLLISIONS
	uint32_t adjust;
#endif
	
	key_entity_t offset;

	/* Hash plugin in use */
	reiser4_plug_t *hash;
};

typedef struct dir40 dir40_t;
#endif

