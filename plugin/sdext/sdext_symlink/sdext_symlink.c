/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_symlink.c -- symlink stat data extension plugin. */

#include <reiser4/plugin.h>
#ifdef ENABLE_SYMLINKS

reiser4_core_t *sdext_symlink_core = NULL;

static uint32_t sdext_symlink_length(stat_entity_t *stat, void *hint) {
	char *name;
	
	name = (hint != NULL) ? hint : stat_body(stat);
	return aal_strlen(name) + 1;
}

static errno_t sdext_symlink_open(stat_entity_t *stat, void *hint) {
	char *data;
	uint32_t len;
	
	aal_assert("umka-1483", stat != NULL);
	aal_assert("umka-1484", hint != NULL);

	data = (char *)stat_body(stat);
	len = aal_strlen(data);
	
	aal_memcpy(hint, data, len);
	*((char *)hint + len) = '\0';
	
	return 0;
}

#ifndef ENABLE_MINIMAL
static errno_t sdext_symlink_init(stat_entity_t *stat, void *hint) {
	uint32_t len;
	
	aal_assert("umka-1481", stat != NULL);
	aal_assert("umka-1482", hint != NULL);

	len = aal_strlen((char *)hint);

	aal_memcpy(stat_body(stat), hint, len);
	*((char *)stat_body(stat) + len) = '\0';
	
	return 0;
}

extern errno_t sdext_symlink_check_struct(stat_entity_t *stat, 
					  repair_hint_t *hint);

extern void sdext_symlink_print(stat_entity_t *stat, 
				aal_stream_t *stream, 
				uint16_t options);

#endif

static reiser4_sdext_ops_t sdext_symlink_ops = {
	.open	 	= sdext_symlink_open,
		
#ifndef ENABLE_MINIMAL
	.init	 	= sdext_symlink_init,
	.print   	= sdext_symlink_print,
	.check_struct   = sdext_symlink_check_struct,
#endif		
	.length	 = sdext_symlink_length
};

static reiser4_plug_t sdext_symlink_plug = {
	.cl    = class_init,
	.id    = {SDEXT_SYMLINK_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "sdext_symlink",
	.desc  = "Symlink stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_symlink_ops
	}
};

static reiser4_plug_t *sdext_symlink_start(reiser4_core_t *c) {
	sdext_symlink_core = c;
	return &sdext_symlink_plug;
}

plug_register(sdext_symlink, sdext_symlink_start, NULL);
#endif
