/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40.h -- default block allocator plugin for reiser4. */

#ifndef ALLOC40_H
#define ALLOC40_H
#ifndef ENABLE_STAND_ALONE

#include <aal/aal.h>
#include <aux/bitmap.h>
#include <reiser4/plugin.h>

#define CRC_SIZE (4)

struct alloc40 {
	reiser4_plug_t *plug;
	
	uint32_t blksize;
	aal_device_t *device;
	aux_bitmap_t *bitmap;

	char *crc;
	int dirty;
};

typedef struct alloc40 alloc40_t;

extern errno_t callback_valid(void *entity, blk_t start,
			      count_t width, void *data);

extern int alloc40_occupied(generic_entity_t *entity, 
			    uint64_t start, uint64_t count);

extern errno_t alloc40_layout(generic_entity_t *entity,
			      region_func_t region_func,
			      void *data) ;
#endif
#endif

