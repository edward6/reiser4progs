/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lw.c -- light weight stat data extension plugin, that implements base
   stat data fields. */

#include "sdext_lw.h"
#include <aux/aux.h>

/* Loads extension to passed @hint */
static errno_t sdext_lw_open(void *body, void *hint) {
	sdext_lw_t *ext;
	sdext_lw_hint_t *sdext_lw;
    
	aal_assert("umka-1188", body != NULL);
	aal_assert("umka-1189", hint != NULL);

	ext = (sdext_lw_t *)body;
	sdext_lw = (sdext_lw_hint_t *)hint;
    
	sdext_lw->mode = sdext_lw_get_mode(ext);
	sdext_lw->nlink = sdext_lw_get_nlink(ext);
	sdext_lw->size = sdext_lw_get_size(ext);
    
	return 0;
}

static uint16_t sdext_lw_length(void *body) {
	return sizeof(sdext_lw_t);
}

#ifndef ENABLE_STAND_ALONE
/* Saves all extension fields from passed @hint to @body. */
static errno_t sdext_lw_init(void *body, 
			     void *hint) 
{
	sdext_lw_hint_t *sdext_lw;
    
	aal_assert("umka-1186", body != NULL);
	aal_assert("umka-1187", hint != NULL);
	
	sdext_lw = (sdext_lw_hint_t *)hint;
    
	sdext_lw_set_mode((sdext_lw_t *)body,
			  sdext_lw->mode);
	
	sdext_lw_set_nlink((sdext_lw_t *)body,
			   sdext_lw->nlink);
	
	sdext_lw_set_size((sdext_lw_t *)body,
			  sdext_lw->size);

	return 0;
}

extern errno_t sdext_lw_check_struct(sdext_entity_t *sdext, uint8_t mode);

extern void sdext_lw_print(void *body, aal_stream_t *stream, uint16_t options);

#endif

static reiser4_sdext_ops_t sdext_lw_ops = {
	.open	 	= sdext_lw_open,
		
#ifndef ENABLE_STAND_ALONE
	.init	 	= sdext_lw_init,
	.print   	= sdext_lw_print,
	.check_struct   = sdext_lw_check_struct,
#endif		
	.length	 	= sdext_lw_length
};

static reiser4_plug_t sdext_lw_plug = {
	.cl    = class_init,
	.id    = {SDEXT_LW_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_lw",
	.desc  = "Light stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_lw_ops
	}
};

static reiser4_plug_t *sdext_lw_start(reiser4_core_t *c) {
	return &sdext_lw_plug;
}

plug_register(sdext_lw, sdext_lw_start, NULL);

