
/*
  alloc40_repair.c -- repair default block allocator plugin methods.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "alloc40.h"

/* Call func for all blocks which belong to the same bitmap block as blk. */
errno_t alloc40_related_region(object_entity_t *entity, blk_t blk, 
    block_func_t func, void *data) 
{
    uint64_t size, i;
    alloc40_t *alloc;
    aal_device_t *device;
    
    aal_assert("vpf-554", entity != NULL, return -1);
    aal_assert("umka-1746", func != NULL, return -1);

    alloc = (alloc40_t *)entity;
	
    aal_assert("vpf-710", alloc->bitmap != NULL, return -1);
    aal_assert("vpf-711", alloc->device != NULL, return -1);
	
    size = aal_device_get_bs(alloc->device) - CRC_SIZE;
    
    for (i = blk / size; i < blk / size + size; i++) {
	errno_t res;
		
	if ((res = func(entity, i, data)))
	    return res;
    }

    return 0;    
}

