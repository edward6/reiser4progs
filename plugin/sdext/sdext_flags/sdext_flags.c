/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_flags.c -- inode flags stat data extension plugin. */

#include "sdext_flags.h"

reiser4_core_t *sdext_flags_core = NULL;

/* Stat data extension length. */
static uint32_t sdext_flags_length(stat_entity_t *stat, void *hint) {
	return sizeof(sdext_flags_t);
}

#ifndef ENABLE_MINIMAL
/* Loads all extension fields to passed @hint. */
static errno_t sdext_flags_open(stat_entity_t *stat, void *hint) {
	sdext_flags_t *ext;
	sdhint_flags_t *flagsh;
    
	aal_assert("umka-3077", stat != NULL);
	aal_assert("umka-3078", hint != NULL);

	ext = (sdext_flags_t *)stat_body(stat);
	flagsh = (sdhint_flags_t *)hint;
	flagsh->flags = sdext_flags_get_flags(ext);
    
	return 0;
}

/* Saves all fields to passed extension @body. */
static errno_t sdext_flags_init(stat_entity_t *stat, void *hint) {
	sdhint_flags_t *flagsh;
    
	aal_assert("umka-3079", stat != NULL);
	aal_assert("umka-3080", hint != NULL);
	
	flagsh = (sdhint_flags_t *)hint;
    
	sdext_flags_set_flags((sdext_flags_t *)stat_body(stat),
			      flagsh->flags);

	return 0;
}

extern void sdext_flags_print(stat_entity_t *stat, 
			      aal_stream_t *stream,
			      uint16_t options);

extern errno_t sdext_flags_check_struct(stat_entity_t *stat,
					repair_hint_t *hint);
#endif

reiser4_sdext_plug_t sdext_flags_plug = {
	.p = {
		.id    = {SDEXT_FLAGS_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sdext_flags",
		.desc  = "Inode flags stat data extension plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.open	   	= sdext_flags_open,
	.init	   	= sdext_flags_init,
	.print     	= sdext_flags_print,
	.check_struct	= sdext_flags_check_struct,
#else
	.open	   	= NULL,
#endif
	.info		= NULL,
	.length	   	= sdext_flags_length
};
