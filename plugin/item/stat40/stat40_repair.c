/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40_repair.c -- reiser4 default stat data plugin. */

#ifndef ENABLE_STAND_ALONE
#include "stat40.h"
#include <repair/plugin.h>

struct pos_hint {
	sdext_entity_t sdext;
	uint8_t mode;
};

static errno_t callback_check_ext(sdext_entity_t *sdext, uint16_t extmask, 
				  void *data) 
{
	struct pos_hint *hint = (struct pos_hint *)data;
	
	hint->sdext = *sdext;
	
	if (!sdext->plug->o.sdext_ops->check_struct)
		return 0;
	
	return plug_call(sdext->plug->o.sdext_ops, check_struct,
			 sdext, hint->mode);
}

errno_t stat40_check_struct(place_t *place, uint8_t mode) {
	struct pos_hint hint;
	errno_t res;
	
	aal_assert("vpf-775", place != NULL);
	
	aal_memset(&hint, 0, sizeof(struct pos_hint));
	
	if ((res = stat40_traverse(place, callback_check_ext, &hint)) < 0)
		return res;
	
	if (res || !hint.sdext.plug) {
		aal_exception_error("Node (%llu), item (%u): does not look like a "
				    "valid stat data.", place->block->nr, 
				    place->pos.item);
		
		return RE_FATAL;
	}
	
	/* Hint is set up by callback, so the last extension lenght has not been
	   added yet. 
	hint.sdext.offset += plug_call(hint.sdext.plug->o.sdext_ops, length, 
				       hint.sdext.body);
	*/
	
	aal_assert("vpf-784", hint.sdext.offset <= place->len);
	
	if (hint.sdext.offset < place->len) {
		aal_exception_error("Node (%llu), item (%u): item has a wrong "
				    "length (%u). Should be (%u). %s", 
				    place->block->nr, place->pos.item, 
				    place->len, hint.sdext.offset, 
				    mode == RM_BUILD ? "Fixed." : "");
		
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		place->len = hint.sdext.offset;
		place_mkdirty(place);
		return 0;
	}
	
	return 0;
}

#endif
