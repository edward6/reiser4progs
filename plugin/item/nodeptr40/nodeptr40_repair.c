/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40_repair.c -- repair default node pointer item plugin methods. */

#ifndef ENABLE_STAND_ALONE
#include "nodeptr40.h"
#include <repair/plugin.h>

errno_t nodeptr40_check_layout(place_t *place, region_func_t region_func, 
			       void *data, uint8_t mode) 
{
	nodeptr40_t *nodeptr;
	blk_t blk;
	errno_t res;
	
	aal_assert("vpf-721", place != NULL);
	
	nodeptr = nodeptr40_body(place);
	
	blk = np40_get_ptr(nodeptr);
	
	res = region_func(place, blk, 1, data);
	
	if (res > 0) {
		if (mode == RM_BUILD) {
			aal_exception_error("Node (%llu), item (%u): a pointer to "
					    "the region [%llu..%llu] is removed.", 
					    place->block->nr, place->pos.item,
					    blk, blk);
			place->len = 0;
			return RE_FIXED;
		}
		
		return RE_FATAL;
	} else if (res < 0) {
		return res;
	}
	
	return RE_OK;
}

errno_t nodeptr40_check_struct(place_t *place, uint8_t mode) {
	aal_assert("vpf-751", place != NULL);
	return place->len != sizeof(nodeptr40_t) ? RE_FATAL : RE_OK;
}
#endif

