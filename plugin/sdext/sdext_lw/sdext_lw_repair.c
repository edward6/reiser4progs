/*
  sdext_lw_repair.c -- light weight stat data extention plugin recovery code.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "sdext_lw.h"
#include <repair/plugin.h>

errno_t sdext_lw_check(sdext_entity_t *sdext, uint8_t mode) {
    aal_assert("vpf-777", sdext != NULL);
    aal_assert("vpf-783", sdext->plugin != NULL);

    if (sdext->offset + sizeof(sdext_lw_t) > sdext->sdlen) {
	aal_exception_error("Does not look like a valid (%s) statdata "
	    "extention.", sdext->plugin->h.label);

	return 	REPAIR_FATAL;
    }
    
    return REPAIR_OK;
}

#endif

