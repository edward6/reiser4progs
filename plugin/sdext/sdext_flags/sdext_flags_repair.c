/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_flags_repair.c -- inode flags stat data extension plugin recovery
   code. */

#ifndef ENABLE_STAND_ALONE
#include "sdext_flags.h"
#include <repair/plugin.h>

errno_t sdext_flags_check_struct(sdext_entity_t *sdext, uint8_t mode) {
	aal_assert("umka-3081", sdext != NULL);
	aal_assert("umka-3082", sdext->plug != NULL);
	
	if (sdext->offset + sizeof(sdext_flags_t) > sdext->sdlen) {
		aal_error("Does not look like a valid (%s) statdata "
			  "extension.", sdext->plug->label);
		return RE_FATAL;
	}
	
	return 0;
}

/* Prints extension into passed @stream. */
void sdext_flags_print(void *body, aal_stream_t *stream,
		       uint16_t options)
{
	sdext_flags_t *ext;
	
	aal_assert("umka-3083", body != NULL);
	aal_assert("umka-3084", stream != NULL);

	ext = (sdext_flags_t *)body;

	aal_stream_format(stream, "flags:\t\t%u\n",
			  sdext_flags_get_flags(ext));
}

#endif
