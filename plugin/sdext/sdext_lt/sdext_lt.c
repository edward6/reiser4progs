/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt.c -- large time stat data extension plugin. */

#include "sdext_lt.h"

/* Stat data extension length. */
static uint16_t sdext_lt_length(stat_entity_t *stat, void *h) {
	return sizeof(sdext_lt_t);
}

#ifndef ENABLE_STAND_ALONE
/* Loads all extension fields to passed @hint. */
static errno_t sdext_lt_open(stat_entity_t *stat, void *hint) {
	sdext_lt_t *ext;
	sdext_lt_hint_t *sdext_lt;
    
	aal_assert("umka-1477", stat != NULL);
	aal_assert("umka-1478", hint != NULL);

	ext = (sdext_lt_t *)stat_body(stat);
	sdext_lt = (sdext_lt_hint_t *)hint;
    
	sdext_lt->atime = sdext_lt_get_atime(ext);
	sdext_lt->mtime = sdext_lt_get_mtime(ext);
	sdext_lt->ctime = sdext_lt_get_ctime(ext);
    
	return 0;
}

/* Saves all fields to passed extension @body. */
static errno_t sdext_lt_init(stat_entity_t *stat, void *hint) {
	sdext_lt_hint_t *sdext_lt;
	sdext_lt_t *ext;
    
	aal_assert("umka-1475", stat != NULL);
	aal_assert("umka-1476", hint != NULL);
	
	sdext_lt = (sdext_lt_hint_t *)hint;
	ext = (sdext_lt_t *)stat_body(stat);
	
	sdext_lt_set_atime(ext, sdext_lt->atime);	
	sdext_lt_set_mtime(ext, sdext_lt->mtime);
	sdext_lt_set_ctime(ext, sdext_lt->ctime);

	return 0;
}

extern errno_t sdext_lt_check_struct(stat_entity_t *stat, 
				     repair_hint_t *hint);

extern void sdext_lt_print(stat_entity_t *stat, 
			   aal_stream_t *stream, 
			   uint16_t options);

#endif

static reiser4_sdext_ops_t sdext_lt_ops = {
#ifndef ENABLE_STAND_ALONE
	.open	   	= sdext_lt_open,
	.init	   	= sdext_lt_init,
	.print     	= sdext_lt_print,
	.check_struct	= sdext_lt_check_struct,
#else
	.open	   	= NULL,
#endif
	.length	   	= sdext_lt_length
};

static reiser4_plug_t sdext_lt_plug = {
	.cl    = class_init,
	.id    = {SDEXT_LT_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_lt",
	.desc  = "Large times stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_lt_ops
	}
};

static reiser4_plug_t *sdext_lt_start(reiser4_core_t *c) {
	return &sdext_lt_plug;
}

plug_register(sdext_lt, sdext_lt_start, NULL);
