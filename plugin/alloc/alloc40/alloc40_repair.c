/*
  alloc40_repair.c -- repair default block allocator plugin methods.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "alloc40.h"
#include <repair/repair_plugin.h>

extern errno_t callback_check_bitmap(object_entity_t *entity, uint64_t blk, 
    void *data);

extern errno_t alloc40_layout(object_entity_t *entity, block_func_t func,
    void *data);

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
    return func(entity, (blk/size) * size, size, data);
}

static errno_t callback_check_layout(object_entity_t *entity, uint64_t blk, 
    void *data) 
{
    alloc40_t *alloc = (alloc40_t *)entity;
    uint32_t size, offset;
    uint8_t *mode = (uint8_t *)data;
    errno_t res;

    res = callback_check_bitmap(entity, blk, data);
    
    if (res == -EINVAL && *mode == REPAIR_REBUILD) {
	/* Data are corrupted. */
	size = aal_device_get_bs(alloc->device) - CRC_SIZE;
	offset = (blk / size / 8) * size;	
	aux_bitmap_mark_region(alloc->bitmap, offset, size);	
    }
    
    return res;
}

errno_t alloc40_check(object_entity_t *entity, uint8_t mode) {
    alloc40_t *alloc = (alloc40_t *)entity;

    aal_assert("vpf-865", alloc != NULL);
    aal_assert("vpf-866", alloc->bitmap != NULL);

    /* Calling layout function for traversing all the bitmap blocks with
       checking callback function.
    */
    return alloc40_layout(entity, callback_check_layout, &mode);

}

#endif
