/*
  alloc36.h -- Space allocator plugin for reiser3.6.x.

  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#ifndef ALLOC36_H
#define ALLOC36_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct reiser4_alloc36 {
	reiser4_plugin_t *plugin;

	object_entity_t *format;
	reiser4_plugin_t *format_plugin;
};

typedef struct reiser4_alloc36 reiser4_alloc36_t;

#endif
