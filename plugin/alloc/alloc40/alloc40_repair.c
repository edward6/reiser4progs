/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   alloc40_repair.c -- repair default block allocator plugin methods. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "alloc40.h"
#include <repair/plugin.h>

struct alloc_hint {
	uint8_t mode;
	errno_t error;
};

extern errno_t callback_check_bitmap(object_entity_t *entity, uint64_t blk, 
				     void *data);

extern errno_t alloc40_layout(object_entity_t *entity, block_func_t func,
			      void *data);

extern errno_t alloc40_related(object_entity_t *entity, blk_t blk, 
			       region_func_t func, void *data);

static errno_t callback_check_layout(object_entity_t *entity, uint64_t blk, 
				     void *data) 
{
	struct alloc_hint *hint = (struct alloc_hint *)data;
	alloc40_t *alloc = (alloc40_t *)entity;
	uint32_t size, offset;
	errno_t res;
	
	res = callback_check_bitmap(entity, blk, data);
	
	if (res == -ESTRUCT) {
		/* Rebuild scans everything first, before rebuilding, so mark 
		   as much as possible to be scanned. Fixable scans only what 
		   is reachable through the internal tree, so leave it as is 
		   to be fixed later. */
		if (hint->mode == REPAIR_REBUILD) {
			/* Data are corrupted. */
			size = aal_device_get_bs(alloc->device) - CRC_SIZE;
			offset = (blk / size / 8) * size;
			aux_bitmap_mark_region(alloc->bitmap, offset, size);
		} else {
			hint->error = REPAIR_FIXABLE;
		}
		
		res = 0;
	}
	
	return res;
}

errno_t alloc40_check_struct(object_entity_t *entity, uint8_t mode) {    
	struct alloc_hint hint;
	alloc40_t *alloc = (alloc40_t *)entity;
	errno_t error;
	
	aal_assert("vpf-865", alloc != NULL);
	aal_assert("vpf-866", alloc->bitmap != NULL);
	
	/* Calling layout function for traversing all the bitmap blocks with
	   checking callback function. */
	hint.mode = mode;
	hint.error = 0;
	error = alloc40_layout(entity, callback_check_layout, &hint);
	
	if (hint.error == REPAIR_FIXABLE && mode == REPAIR_FIX) {
		aal_exception_warn("Checksums will be fixed later.\n");
		return 0;
	}
	
	return error;
}

#endif

