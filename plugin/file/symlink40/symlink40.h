/*
  symlink40.h -- reiser4 symlink plugin structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef SYMLINK40_H
#define SYMLINK40_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <sys/stat.h>

#include <reiser4/plugin.h>
#include <plugin/file/file40/file40.h>

/* Compaund directory structure */
struct symlink40 {

	/*
	  Common file fiedls (statdata, etc). As symlink40 has nothing but
	  statdata only, this structure has only file handler, which contains
	  stuff for statdata handling.
	*/
	file40_t file;

	/* Parent key */
	key_entity_t parent;
};

typedef struct symlink40 symlink40_t;

#endif

