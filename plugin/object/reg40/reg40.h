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

extern reiser4_plug_t reg40_plug;
extern reiser4_core_t *reg40_core;

extern int64_t reg40_put(object_entity_t *entity,
			 void *buff, uint64_t n);

extern errno_t reg40_seek(object_entity_t *entity,
			  uint64_t offset);

extern errno_t reg40_reset(object_entity_t *entity);
extern uint64_t reg40_offset(object_entity_t *entity);
extern lookup_t reg40_update_body(object_entity_t *entity);

extern reiser4_plug_t *reg40_policy_plug(reg40_t *reg,
					 uint64_t new_size);
#endif

