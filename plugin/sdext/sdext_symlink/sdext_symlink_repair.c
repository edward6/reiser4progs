/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_symlink.c -- symlink stat data extention plugin. */

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
			aal_exception_error("Does not look like a valid (%s) "
					    "statdata extention.", 
					    sdext->plug->label);
			
			return REPAIR_FATAL;
		}
	}
	
	return REPAIR_OK;
}

#endif
#endif

