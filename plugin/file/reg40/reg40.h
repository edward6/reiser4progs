/*
  reg40.h -- reiser4 regular file plugin structures.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REG40_H
#define REG40_H

#include <aal/aal.h>
#include <sys/stat.h>
#include <reiser4/plugin.h>
#include <plugin/file/object40/object40.h>

/* Compaund directory structure */
struct reg40 {

	/* Common fields (statdata, etc) */
	object40_t file;

	/* Current body item coord stored here */
	place_t body;

	/* Current position in the directory */
	uint64_t offset;
};

typedef struct reg40 reg40_t;

#endif

