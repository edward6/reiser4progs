/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40.h -- reiser4 symlink plugin structures. */

#ifndef SYM40_H
#define SYM40_H

#ifndef ENABLE_MINIMAL
#  include <time.h>
#  include <unistd.h>
#  include <limits.h>
#endif

#include <aal/libaal.h>
#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

/* Symlink struct. */
typedef struct sym40 {
	/* Common file fiedls (statdata, etc). As sym40 has nothing but statdata
	   only, this structure has only file handler, which contains stuff for
	   statdata handling. */
	obj40_t obj;
} sym40_t;

extern reiser4_core_t *sym40_core;
extern reiser4_plug_t sym40_plug;

extern object_entity_t *sym40_open(object_info_t *info);
#endif

