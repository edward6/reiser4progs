/*
  oid40.h -- reiser4 default oid allocator structures.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef OID40_H
#define OID40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

#define OID40_HYPER_LOCALITY            0x26
#define OID40_ROOT_LOCALITY             0x29
#define OID40_ROOT_OBJECTID             0x2a

#define OID40_RESERVED                  (1 << 16)

struct oid40 {
	reiser4_plugin_t *plugin;

	void *start;
	uint32_t len;

	uint64_t next;
	uint64_t used;

	int dirty;
};

typedef struct oid40 oid40_t;

#define oid40_get_next(area)            LE64_TO_CPU(*((d64_t *)area))
#define oid40_set_next(area, val)       (*((d64_t *)area) = CPU_TO_LE64(val))

#define oid40_get_used(area)            LE64_TO_CPU(*(((d64_t *)area) + 1))
#define oid40_set_used(area, val)       (*(((d64_t *)area) + 1) = CPU_TO_LE64(val))

#endif

