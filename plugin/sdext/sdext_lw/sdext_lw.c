/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lw.c -- light weight stat data extension plugin, that implements base
   stat data fields. */

#include "sdext_lw.h"
#include <aux/aux.h>

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

static uint16_t sdext_lw_length(stat_entity_t *stat, void *hint) {
	return sizeof(sdext_lw_t);
}

#ifndef ENABLE_STAND_ALONE
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

