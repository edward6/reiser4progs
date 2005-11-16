/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt.c -- large time stat data extension plugin. */

#include "sdext_lt.h"

reiser4_core_t *sdext_lt_core = NULL;

/* Stat data extension length. */
static uint32_t sdext_lt_length(stat_entity_t *stat, void *h) {
	return sizeof(sdext_lt_t);
}

#ifndef ENABLE_MINIMAL
/* Loads all extension fields to passed @hint. */
static errno_t sdext_lt_open(stat_entity_t *stat, void *hint) {
	sdext_lt_t *ext;
	sdhint_lt_t *lth;
    
	aal_assert("umka-1477", stat != NULL);
	aal_assert("umka-1478", hint != NULL);

	ext = (sdext_lt_t *)stat_body(stat);
	lth = (sdhint_lt_t *)hint;
    
	lth->atime = sdext_lt_get_atime(ext);
	lth->mtime = sdext_lt_get_mtime(ext);
	lth->ctime = sdext_lt_get_ctime(ext);
    
	return 0;
}

/* Saves all fields to passed extension @body. */
static errno_t sdext_lt_init(stat_entity_t *stat, void *hint) {
	sdhint_lt_t *lth;
	sdext_lt_t *ext;
    
	aal_assert("umka-1475", stat != NULL);
	aal_assert("umka-1476", hint != NULL);
	
	lth = (sdhint_lt_t *)hint;
	ext = (sdext_lt_t *)stat_body(stat);
	
	sdext_lt_set_atime(ext, lth->atime);
	sdext_lt_set_mtime(ext, lth->mtime);
	sdext_lt_set_ctime(ext, lth->ctime);

	return 0;
}

extern errno_t sdext_lt_check_struct(stat_entity_t *stat, 
				     repair_hint_t *hint);

extern void sdext_lt_print(stat_entity_t *stat, 
			   aal_stream_t *stream, 
			   uint16_t options);

#endif

reiser4_sdext_plug_t sdext_lt_plug = {
	.p = {
		.id    = {SDEXT_LT_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sdext_lt",
		.desc  = "Large times stat data extension plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.open	   	= sdext_lt_open,
	.init	   	= sdext_lt_init,
	.print     	= sdext_lt_print,
	.check_struct	= sdext_lt_check_struct,
#else
	.open	   	= NULL,
#endif
	.info		= NULL,
	.length	   	= sdext_lt_length
};
