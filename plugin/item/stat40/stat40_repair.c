/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40_repair.c -- reiser4 default stat data plugin. */

#ifndef ENABLE_STAND_ALONE
#include "stat40.h"
#include <repair/plugin.h>

typedef struct repair_stat_hint {
	uint64_t extmask;
	uint64_t goodmask;
	uint8_t mode;
} repair_stat_hint_t;

static errno_t callback_check_ext(sdext_entity_t *sdext, 
				  uint64_t extmask, void *data) 
{
	repair_stat_hint_t *hint = (repair_stat_hint_t *)data;
	uint8_t chunk;
	
	/* Set read extmask. */
	if (!sdext->plug) {
		hint->extmask |= ((uint64_t)extmask);
		return 0;
	}

	/* Extention plugin was found, set the bit into @goodmask. */
	hint->goodmask |= ((uint64_t)1 << sdext->plug->id.id);
	chunk = sdext->plug->id.id / 16;
	if (chunk > 0) {
		hint->goodmask |= (1 << (chunk * 16 - 1));
	}
	
	if (!sdext->plug->o.sdext_ops->check_struct)
		return 0;
	
	return plug_call(sdext->plug->o.sdext_ops, check_struct,
			 sdext, hint->mode);
}

static errno_t callback_fix_mask(sdext_entity_t *sdext, 
				 uint64_t extmask, void *data) 
{
	uint64_t *mask = (uint64_t *)data;

	if (sdext->plug) 
		return 0;

	/* This time the callback is called for the extmask. Fix it. */
	*((uint16_t *)sdext->body) = *(uint16_t *)mask;

	(*mask) >>= 16;
	return 0;
}

errno_t stat40_check_struct(reiser4_place_t *place, uint8_t mode) {
	repair_stat_hint_t hint;
	sdext_entity_t sdext;
	errno_t res;
	
	aal_assert("vpf-775", place != NULL);
	
	aal_memset(&hint, 0, sizeof(hint));
	hint.mode = mode;
	
	if ((res = stat40_traverse(place, callback_check_ext, 
				   &sdext, &hint)) < 0)
		return res;
	
	if (res) {
		aal_error("Node (%llu), item (%u): does not look like a "
			  "valid stat data.", place->node->block->nr,
			  place->pos.item);
		
		return RE_FATAL;
	}
	
	/* Hint is set up by callback, so the last extension lenght has not been
	   added yet.

	   hint.sdext.offset += plug_call(hint.sdext.plug->o.sdext_ops, length,
	   hint.sdext.body);
	*/
	
	if (sdext.offset < place->len) {
		aal_error("Node (%llu), item (%u): item has the wrong "
			  "length (%u). Should be (%u). %s", 
			  place->node->block->nr, place->pos.item, 
			  place->len, sdext.offset, 
			  mode == RM_BUILD ? "Fixed." : "");
		
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		place->len = sdext.offset;
		place_mkdirty(place);
	}
	
	/* Check the extention mask. */
	if (hint.extmask != hint.goodmask) {
		aal_error("Node (%llu), item (%u): item has the wrong "
			  "extention mask (%llu). Should be (%llu). %s",
			  place->node->block->nr, place->pos.item,
			  hint.extmask, hint.goodmask,
			  mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode == RM_CHECK)
			return RE_FIXABLE;

		if ((res = stat40_traverse(place, callback_fix_mask, 
					   &sdext, &hint.goodmask)) < 0)
			return res;

		place_mkdirty(place);
	}
	
	return 0;
}


/* Callback for counting stat data extensions in use. */
static errno_t callback_count_ext(sdext_entity_t *sdext,
				  uint64_t extmask, void *data)
{
	if (!sdext->plug) return 0;

        (*(uint32_t *)data)++;
        return 0;
}

/* This function returns stat data extension count. */
static uint32_t stat40_sdext_count(reiser4_place_t *place) {
	sdext_entity_t sdext;
        uint32_t count = 0;

        if (stat40_traverse(place, callback_count_ext,
			    &sdext, &count) < 0)
                return 0;

        return count;
}

/* Prints extension into @stream. */
static errno_t callback_print_ext(sdext_entity_t *sdext, 
				  uint64_t extmask, void *data)
{
	uint16_t length;
	aal_stream_t *stream;

	stream = (aal_stream_t *)data;

	if (!sdext->plug) {
		aal_stream_format(stream, "mask:\t\t0x%x\n", extmask);
		return 0;
	}
				
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  sdext->plug->label);
	
	aal_stream_format(stream, "offset:\t\t%u\n",
			  sdext->offset);
	
	length = plug_call(sdext->plug->o.sdext_ops,
			   length, sdext->body);
	
	aal_stream_format(stream, "len:\t\t%u\n", length);
	
	plug_call(sdext->plug->o.sdext_ops, print,
		  sdext->body, stream, 0);

	return 0;
}

/* Prints statdata item into passed @stream */
void stat40_print(reiser4_place_t *place, aal_stream_t *stream,
		  uint16_t options)
{
	sdext_entity_t sdext;

	aal_assert("umka-1407", place != NULL);
	aal_assert("umka-1408", stream != NULL);
    
	aal_stream_format(stream, "UNITS=1\nexts:\t\t%u\n", 
			  stat40_sdext_count(place));
	
	stat40_traverse(place, callback_print_ext, &sdext, (void *)stream);
}
#endif
