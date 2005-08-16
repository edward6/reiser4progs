/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40.h -- default block allocator plugin for reiser4. */

#ifndef ALLOC40_H
#define ALLOC40_H
#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include <reiser4/bitmap.h>
#include <reiser4/plugin.h>

#define CRC_SIZE (4)

typedef struct alloc40 {
	reiser4_plug_t *plug;
	
	uint32_t state;
	uint32_t blksize;
	
	aal_device_t *device;
	reiser4_bitmap_t *bitmap;

	char *crc;

	void *data;
} alloc40_t;

extern reiser4_plug_t alloc40_plug;

extern int alloc40_occupied(generic_entity_t *entity, 
			    uint64_t start, uint64_t count);

extern errno_t alloc40_layout(generic_entity_t *entity,
			      region_func_t region_func,
			      void *data);

#endif
#endif
