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


/* Callback for counting stat data extensions in use. */
static errno_t callback_count_ext(sdext_entity_t *sdext,
				  uint16_t extmask, 
				  void *data)
{
        (*(uint32_t *)data)++;
        return 0;
}

/* This function returns stat data extension count. */
static uint32_t stat40_sdext_count(place_t *place) {
	sdext_entity_t sdext;
        uint32_t count = 0;

        if (stat40_traverse(place, callback_count_ext,
			    &sdext, &count) < 0)
	{
                return 0;
	}

        return count;
}

/* Prints extension into @stream. */
static errno_t callback_print_ext(sdext_entity_t *sdext,
				  uint16_t extmask, 
				  void *data)
{
	int print_mask;
	uint16_t length;
	aal_stream_t *stream;

	stream = (aal_stream_t *)data;

	print_mask = (sdext->plug->id.id == 0 ||
		      (sdext->plug->id.id + 1) % 16 == 0);
	
	if (print_mask)	{
		aal_stream_format(stream, "mask:\t\t0x%x\n",
				  extmask);
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
errno_t stat40_print(place_t *place, aal_stream_t *stream,
		     uint16_t options)
{
	sdext_entity_t sdext;

	aal_assert("umka-1407", place != NULL);
	aal_assert("umka-1408", stream != NULL);
    
	aal_stream_format(stream, "STATDATA PLUGIN=%s, LEN=%u, KEY=[%s], "
			  "UNITS=1\n", place->plug->label, place->len,
			  stat40_core->key_ops.print(&place->key, PO_DEFAULT));
		
	aal_stream_format(stream, "exts:\t\t%u\n", stat40_sdext_count(place));
	
	return stat40_traverse(place, callback_print_ext, &sdext,
			       (void *)stream);
}
#endif
