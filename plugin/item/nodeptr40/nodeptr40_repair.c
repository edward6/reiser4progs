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
		aal_error("Node (%llu), item (%u): wrong pointer to "
			  "the block %llu.%s", place->block->nr,
			  place->pos.item, blk, mode == RM_BUILD ?
			  " Removed." : "");

		if (mode == RM_BUILD) {
			place->len = 0;
			return 0;
		}
		
		return RE_FATAL;
	} else if (res < 0) {
		return res;
	}
	
	return 0;
}

errno_t nodeptr40_check_struct(place_t *place, uint8_t mode) {
	aal_assert("vpf-751", place != NULL);
	return place->len != sizeof(nodeptr40_t) ? RE_FATAL : 0;
}

/* Prints passed nodeptr into @stream */
errno_t nodeptr40_print(place_t *place, aal_stream_t *stream,
			uint16_t options) 
{
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-544", place != NULL);
	aal_assert("umka-545", stream != NULL);
    
	nodeptr = nodeptr40_body(place);

	aal_stream_format(stream, "NODEPTR PLUGIN=%s, LEN=%u, "
			  "KEY=[%s], UNITS=1\n[%llu]\n",
			  place->plug->label, place->len, 
			  nodeptr40_core->key_ops.print(&place->key,
							PO_DEFAULT), 
			  np40_get_ptr(nodeptr));
	
	return 0;
}
#endif
