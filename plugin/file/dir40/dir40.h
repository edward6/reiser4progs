/*
  dir40.h -- reiser4 hashed directory plugin structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef DIR40_H
#define DIR40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>
#include <plugin/file/file40.h>

/* Compaund directory structure */
struct dir40 {
	
	/* Common fields for all files (statdata, etc) */
	file40_t file;

	/* Current body item coord */
	reiser4_place_t body;

	/* Current position in the directory */
	uint32_t offset;

	/* Hash plugin in use */
	reiser4_plugin_t *hash;
};

typedef struct dir40 dir40_t;

#endif

