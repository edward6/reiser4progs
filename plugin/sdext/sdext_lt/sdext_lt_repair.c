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
		aal_exception_error("Does not look like a valid (%s) statdata "
				    "extension.", sdext->plug->label);
		return RE_FATAL;
	}
	
	return 0;
}
#endif

