/*
  sdext_symlink.c -- symlink stat data extention plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include <reiser4/plugin.h>
#include <repair/repair_plugin.h>

errno_t sdext_symlink_check(sdext_entity_t *sdext, uint8_t mode) {
    uint32_t offset;
    
    aal_assert("vpf-779", sdext != NULL);
    aal_assert("vpf-780", sdext->plugin != NULL);
    
    offset = 0;
    
    while (*((char *)sdext->body + offset)) {
	offset++;
	if (offset == sdext->sdlen) {
	    aal_exception_error("Does not look like a valid (%s) statdata "
		"extention.", sdext->plugin->h.label);
	    
	    return REPAIR_FATAL;
	}
    }
    
    return REPAIR_OK;
}

#endif

