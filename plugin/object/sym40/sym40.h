/*
  sym40.h -- reiser4 symlink plugin structures.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef SYM40_H
#define SYM40_H

#ifndef ENABLE_SYMLINKS_SUPPORT

#include <aal/aal.h>
#include <aux/aux.h>
#include <sys/stat.h>

#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

/* Compaund directory structure */
struct sym40 {

	/*
	  Common file fiedls (statdata, etc). As symlink40 has nothing but
	  statdata only, this structure has only file handler, which contains
	  stuff for statdata handling.
	*/
	obj40_t obj;

	/* Parent key */
	key_entity_t parent;
};

typedef struct sym40 sym40_t;

#endif

#endif

