/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_symlink.c -- symlink stat data extension plugin. */

#ifdef ENABLE_SYMLINKS
#ifndef ENABLE_STAND_ALONE

#include <reiser4/plugin.h>
#include <repair/plugin.h>

errno_t sdext_symlink_check_struct(sdext_entity_t *sdext, uint8_t mode) {
	uint32_t offset;
	
	aal_assert("vpf-779", sdext != NULL);
	aal_assert("vpf-780", sdext->plug != NULL);
	
	offset = 0;
	
	while (*((char *)sdext->body + offset)) {
		offset++;
		if (offset == sdext->sdlen) {
			aal_error("Does not look like a valid (%s) "
				  "statdata extension.", sdext->plug->label);
			
			return RE_FATAL;
		}
	}
	
	return 0;
}

void sdext_symlink_print(void *body, aal_stream_t *stream, uint16_t options) {
	aal_assert("umka-1485", body != NULL);
	aal_assert("umka-1486", stream != NULL);

	aal_stream_format(stream, "len:\t\t%u\n",
			  aal_strlen((char *)body));
	
	aal_stream_format(stream, "data:\t\t\"%s\"\n",
			  (char *)body);
}

#endif
#endif

