/*
  alloc40.h -- default block allocator plugin for reiser4.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ALLOC40_H
#define ALLOC40_H

#include <aal/aal.h>
#include <aux/bitmap.h>
#include <reiser4/plugin.h>

struct alloc40 {
	reiser4_plugin_t *plugin;
	
	aux_bitmap_t *bitmap;
	object_entity_t *format;

	char *crc;
};

typedef struct alloc40 alloc40_t;

#endif

