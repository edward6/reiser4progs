/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40_repair.c -- repair default node pointer item plugin methods. */

#ifndef ENABLE_MINIMAL
#include "nodeptr40.h"
#include <repair/plugin.h>

errno_t nodeptr40_check_layout(reiser4_place_t *place, repair_hint_t *hint,
			       region_func_t region_func, void *data) 
{
	nodeptr40_t *nodeptr;
	blk_t blk;
	errno_t res;
	
	aal_assert("vpf-721", place != NULL);
	
	nodeptr = nodeptr40_body(place);
	
	blk = np40_get_ptr(nodeptr);
	
	res = region_func(place, blk, 1, data);
	
	if (res > 0) {
		fsck_mess("Node (%llu), item (%u): wrong pointer to "
			  "the block %llu.%s", place_blknr(place),
			  place->pos.item, blk, hint->mode == RM_BUILD ?
			  " Removed." : "");

		if (hint->mode == RM_BUILD) {
			hint->len = place->len;
			return 0;
		}
		
		return RE_FATAL;
	} else if (res < 0) {
		return res;
	}
	
	return 0;
}

errno_t nodeptr40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	aal_assert("vpf-751", place != NULL);
	return place->len != sizeof(nodeptr40_t) ? RE_FATAL : 0;
}

/* Prints passed nodeptr into @stream */
void nodeptr40_print(reiser4_place_t *place, aal_stream_t *stream,
		     uint16_t options)
{
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-544", place != NULL);
	aal_assert("umka-545", stream != NULL);
    
	nodeptr = nodeptr40_body(place);

	aal_stream_format(stream, "UNITS=1\n[%llu]\n", 
			  np40_get_ptr(nodeptr));
}
#endif
