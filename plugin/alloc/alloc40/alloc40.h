/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40.h -- default block allocator plugin for reiser4. */

#ifndef ALLOC40_H
#define ALLOC40_H

#ifndef ENABLE_ALONE

#include <aal/aal.h>
#include <aux/bitmap.h>
#include <reiser4/plugin.h>

#define CRC_SIZE (4)

struct alloc40 {
	reiser4_plugin_t *plugin;
	
	int dirty;
	char *crc;

	uint32_t blocksize;
	aal_device_t *device;
	aux_bitmap_t *bitmap;
};

typedef struct alloc40 alloc40_t;

#endif

#endif

