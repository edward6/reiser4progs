/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_flags.c -- inode flags stat data extension plugin. */

#include "sdext_flags.h"

/* Stat data extension length. */
static uint16_t sdext_flags_length(void *body) {
	return sizeof(sdext_flags_t);
}

#ifndef ENABLE_STAND_ALONE
/* Loads all extension fields to passed @hint. */
static errno_t sdext_flags_open(void *body, void *hint) {
	sdext_flags_t *ext;
	sdext_flags_hint_t *sdext_flags;
    
	aal_assert("umka-3077", body != NULL);
	aal_assert("umka-3078", hint != NULL);

	ext = (sdext_flags_t *)body;
	sdext_flags = (sdext_flags_hint_t *)hint;
	sdext_flags->flags = sdext_flags_get_flags(ext);
    
	return 0;
}

/* Saves all fields to passed extension @body. */
static errno_t sdext_flags_init(void *body, void *hint) {
	sdext_flags_hint_t *sdext_flags;
    
	aal_assert("umka-3079", body != NULL);
	aal_assert("umka-3080", hint != NULL);
	
	sdext_flags = (sdext_flags_hint_t *)hint;
    
	sdext_flags_set_flags((sdext_flags_t *)body,
			      sdext_flags->flags);

	return 0;
}

extern void sdext_flags_print(void *body, aal_stream_t *stream,
			      uint16_t options);

extern errno_t sdext_flags_check_struct(sdext_entity_t *sdext,
					uint8_t mode);
#endif

static reiser4_sdext_ops_t sdext_flags_ops = {
#ifndef ENABLE_STAND_ALONE
	.open	   	= sdext_flags_open,
	.init	   	= sdext_flags_init,
	.print     	= sdext_flags_print,
	.check_struct	= sdext_flags_check_struct,
#else
	.open	   	= NULL,
#endif
	.length	   	= sdext_flags_length
};

static reiser4_plug_t sdext_flags_plug = {
	.cl    = class_init,
	.id    = {SDEXT_FLAGS_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_flags",
	.desc  = "Inode flags stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_flags_ops
	}
};

static reiser4_plug_t *sdext_flags_start(reiser4_core_t *c) {
	return &sdext_flags_plug;
}

plug_register(sdext_flags, sdext_flags_start, NULL);
