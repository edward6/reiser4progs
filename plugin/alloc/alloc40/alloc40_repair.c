/*
  alloc40_repair.c -- repair default block allocator plugin methods.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include "alloc40.h"

/*
  Call @func for all blocks which belong to the same bitmap block as passed
  @blk. It is needed for fsck. In the case it detremined that a block is not
  corresponds to its value in block allocator, it should check all the related
  (neighbour) blocks which are described by one bitmap block (4096 - CRC_SIZE).
*/
errno_t alloc40_related_region(object_entity_t *entity, blk_t blk, 
    region_func_t func, void *data) 
{
    uint64_t size;
    alloc40_t *alloc;
    aal_device_t *device;
    
    aal_assert("vpf-554", entity != NULL);
    aal_assert("umka-1746", func != NULL);

    alloc = (alloc40_t *)entity;
	
    aal_assert("vpf-710", alloc->bitmap != NULL);
    aal_assert("vpf-711", alloc->device != NULL);
	
    size = aal_device_get_bs(alloc->device) - CRC_SIZE;
 	
    /* Loop though the all blocks one bitmap block describes and calling
     * passed @func for each of them. */   
    return func(entity, blk/size, size, data);
}

#endif
