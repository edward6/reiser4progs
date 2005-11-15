/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lw.c -- light weight stat data extension plugin, that implements base
   stat data fields. */

#include "sdext_lw.h"
#include <aux/aux.h>

reiser4_core_t *sdext_lw_core = NULL;

/* Loads extension to passed @hint */
static errno_t sdext_lw_open(stat_entity_t *stat, void *hint) {
	sdext_lw_t *ext;
	sdhint_lw_t *lwh;
    
	aal_assert("umka-1188", stat != NULL);
	aal_assert("umka-1189", hint != NULL);

	ext = (sdext_lw_t *)stat_body(stat);
	lwh = (sdhint_lw_t *)hint;
    
	lwh->mode = sdext_lw_get_mode(ext);
	lwh->nlink = sdext_lw_get_nlink(ext);
	lwh->size = sdext_lw_get_size(ext);
    
	return 0;
}

static uint32_t sdext_lw_length(stat_entity_t *stat, void *hint) {
	return sizeof(sdext_lw_t);
}

#ifndef ENABLE_MINIMAL
/* Saves all extension fields from passed @hint to @body. */
static errno_t sdext_lw_init(stat_entity_t *stat, void *hint) {
	sdhint_lw_t *lwh;
	sdext_lw_t *ext;
	
	aal_assert("umka-1186", stat != NULL);
	aal_assert("umka-1187", hint != NULL);
	
	lwh = (sdhint_lw_t *)hint;
	ext = (sdext_lw_t *)stat_body(stat);
	
	sdext_lw_set_mode(ext, lwh->mode);
	sdext_lw_set_nlink(ext, lwh->nlink);
	sdext_lw_set_size(ext, lwh->size);

	return 0;
}

extern errno_t sdext_lw_check_struct(stat_entity_t *stat, 
				     repair_hint_t *hint);

extern void sdext_lw_print(stat_entity_t *stat, 
			   aal_stream_t *stream, 
			   uint16_t options);

#endif

reiser4_sdext_plug_t sdext_lw_plug = {
	.p = {
		.id    = {SDEXT_LW_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sdext_lw",
		.desc  = "Light stat data extension plugin.",
#endif
	},

	.open	 	= sdext_lw_open,
	
#ifndef ENABLE_MINIMAL
	.init	 	= sdext_lw_init,
	.info		= NULL,
	.print   	= sdext_lw_print,
	.check_struct   = sdext_lw_check_struct,
#endif		
	.length	 	= sdext_lw_length
};
