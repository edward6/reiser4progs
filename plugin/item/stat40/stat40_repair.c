/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40_repair.c -- reiser4 default stat data plugin. */

#ifndef ENABLE_STAND_ALONE
#include "stat40.h"
#include <repair/plugin.h>

static errno_t callback_check_ext(sdext_entity_t *sdext, uint16_t extmask, 
				  void *data) 
{
	if (!sdext->plug->o.sdext_ops->check_struct)
		return 0;
	
	return plug_call(sdext->plug->o.sdext_ops, check_struct,
			 sdext, *(uint8_t *)data);
}

errno_t stat40_check_struct(place_t *place, uint8_t mode) {
	sdext_entity_t sdext;
	errno_t res;
	
	aal_assert("vpf-775", place != NULL);
	
	if ((res = stat40_traverse(place, callback_check_ext, 
				   &sdext, &mode)) < 0)
		return res;
	
	if (res) {
		aal_error("Node (%llu), item (%u): does not look like a "
			  "valid stat data.", place->block->nr, place->pos.item);
		
		return RE_FATAL;
	}
	
	/* Hint is set up by callback, so the last extension lenght has not been
	   added yet. 
	hint.sdext.offset += plug_call(hint.sdext.plug->o.sdext_ops, length, 
				       hint.sdext.body);
	*/
	
	if (sdext.offset < place->len) {
		aal_error("Node (%llu), item (%u): item has a wrong "
			  "length (%u). Should be (%u). %s", 
			  place->block->nr, place->pos.item, 
			  place->len, sdext.offset, 
			  mode == RM_BUILD ? "Fixed." : "");
		
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		place->len = sdext.offset;
		place_mkdirty(place);
		return 0;
	}
	
	return 0;
}

#endif
