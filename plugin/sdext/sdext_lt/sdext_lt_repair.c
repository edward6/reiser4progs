/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt_repair.c -- large time stat data extension plugin recovery code. */

#ifndef ENABLE_STAND_ALONE
#include "sdext_lt.h"
#include <repair/plugin.h>

errno_t sdext_lt_check_struct(sdext_entity_t *sdext, uint8_t mode) {
	aal_assert("vpf-776", sdext != NULL);
	aal_assert("vpf-782", sdext->plug != NULL);
	
	if (sdext->offset + sizeof(sdext_lt_t) > sdext->sdlen) {
		aal_error("Does not look like a valid (%s) statdata "
			  "extension.", sdext->plug->label);
		return RE_FATAL;
	}
	
	return 0;
}

/* Prints extension into passed @stream. */
void sdext_lt_print(void *body, aal_stream_t *stream, uint16_t options) {
	sdext_lt_t *ext;
	
	aal_assert("umka-1479", body != NULL);
	aal_assert("umka-1480", stream != NULL);

	ext = (sdext_lt_t *)body;

	aal_stream_format(stream, "atime:\t\t%u\n",
			  sdext_lt_get_atime(ext));
	
	aal_stream_format(stream, "mtime:\t\t%u\n",
			  sdext_lt_get_mtime(ext));
	
	aal_stream_format(stream, "ctime:\t\t%u\n",
			  sdext_lt_get_ctime(ext));
}

#endif
