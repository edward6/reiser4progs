/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   sdext_unix.c -- stat data exception plugin, that implements unix stat data fields. */

#ifndef ENABLE_STAND_ALONE

#include "sdext_unix.h"
#include <repair/plugin.h>

errno_t sdext_unix_check(sdext_entity_t *sdext, uint8_t mode) {
	aal_assert("vpf-778", sdext != NULL);
	aal_assert("vpf-781", sdext->plugin != NULL);
	
	if (sdext->offset + sizeof(sdext_unix_t) > sdext->sdlen) {
		aal_exception_error("Does not look like a valid (%s) statdata "
				    "extention.", sdext->plugin->h.label);
		
		return REPAIR_FATAL;
	}
	
	return REPAIR_OK;
}

#endif

