/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc40_repair.c -- repair default block allocator plugin methods. */

#ifndef ENABLE_STAND_ALONE
#include "alloc40.h"
#include <repair/plugin.h>

extern errno_t callback_valid(object_entity_t *entity, 
			      blk_t start, 
			      count_t width, 
			      void *data);

extern errno_t alloc40_layout(object_entity_t *entity, 
			      region_func_t func,
			      void *data);

extern int alloc40_occupied(generic_entity_t *entity, 
			    uint64_t start, 
			    uint64_t count);

/* Call @func for all blocks which belong to the same bitmap block as passed
   @blk. It is needed for fsck. In the case it detremined that a block is not
   corresponds to its value in block allocator, it should check all the related
   (neighbour) blocks which are described by one bitmap block (4096 -
   CRC_SIZE). */
errno_t alloc40_region(generic_entity_t *entity, blk_t blk, 
		       region_func_t region_func, void *data) 
{
	uint64_t size;
	alloc40_t *alloc;
    
	aal_assert("vpf-554", entity != NULL);
	aal_assert("umka-1746", region_func != NULL);
    
	alloc = (alloc40_t *)entity;
    
	aal_assert("vpf-710", alloc->bitmap != NULL);
	aal_assert("vpf-711", alloc->device != NULL);
    
	size = alloc->blksize - CRC_SIZE;
    
	/* Loop though the all blocks one bitmap block describes and calling
	   passed @region_func for each of them. */   
	return region_func(entity, (blk / size) * size, size, data);
}

struct alloc_hint {
	region_func_t func;
	void *data;
};

static errno_t callback_check_layout(void *entity, blk_t start, 
				     count_t width, void *data) 
{
	struct alloc_hint *hint = (struct alloc_hint *)data;
	errno_t res;
	
	if ((res = callback_valid(entity, start, width, NULL)) < 0)
		return res;
	
	/* If bitmap block looks corrupted or the very first bit is not set,
	   call func for the region */
	if (res || alloc40_occupied(entity, start, 1))
		hint->func(entity, start, width, hint->data);
	
	return 0;
}

errno_t alloc40_layout_bad(object_entity_t *entity,
			   region_func_t func,
			   void *data)
{
	struct alloc_hint hint;
	
	aal_assert("vpf-1323", func != NULL);
	
	hint.func = func;
	hint.data = data;
	
	return alloc40_layout(entity, callback_check_layout, &hint);
}
#endif
