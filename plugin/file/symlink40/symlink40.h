/*
  symlink40.h -- reiser4 symlink plugin structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef SYMLINK40_H
#define SYMLINK40_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/file/file40/file40.h>

/* Compaund directory structure */
struct symlink40 {

	/* Common file fiedls (statdata, etc) */
	file40_t file;
};

typedef struct symlink40 symlink40_t;

#endif

