/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_symlink.c -- symlink stat data extension plugin. */

#ifdef ENABLE_SYMLINKS
#include <reiser4/plugin.h>

static errno_t sdext_symlink_open(void *body, 
				  void *hint) 
{
	char *data;
	uint32_t len;
	
	aal_assert("umka-1483", body != NULL);
	aal_assert("umka-1484", hint != NULL);

	data = (char *)body;
	len = aal_strlen(data);
	
	aal_memcpy(hint, data, len);
	*((char *)hint + len) = '\0';
	
	return 0;
}

static uint16_t sdext_symlink_length(void *body) {
	aal_assert("umka-1488", body != NULL);
	return aal_strlen((char *)body) + 1;
}

#ifndef ENABLE_STAND_ALONE
static errno_t sdext_symlink_init(void *body, 
				  void *hint)
{
	uint32_t len;
	
	aal_assert("umka-1481", body != NULL);
	aal_assert("umka-1482", hint != NULL);

	len = aal_strlen((char *)hint);

	aal_memcpy(body, hint, len);
	*((char *)body + len) = '\0';
	
	return 0;
}

extern errno_t sdext_symlink_print(void *body, aal_stream_t *stream,
				   uint16_t options);

extern errno_t sdext_symlink_check_struct(sdext_entity_t *sdext,
					  uint8_t mode);
#endif

static reiser4_sdext_ops_t sdext_symlink_ops = {
	.open	 	= sdext_symlink_open,
		
#ifndef ENABLE_STAND_ALONE
	.init	 	= sdext_symlink_init,
	.print   	= sdext_symlink_print,
	.check_struct   = sdext_symlink_check_struct,
#endif		
	.length	 = sdext_symlink_length
};

static reiser4_plug_t sdext_symlink_plug = {
	.cl    = class_init,
	.id    = {SDEXT_SYMLINK_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_symlink",
	.desc  = "Symlink stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_symlink_ops
	}
};

static reiser4_plug_t *sdext_symlink_start(reiser4_core_t *c) {
	return &sdext_symlink_plug;
}

plug_register(sdext_symlink, sdext_symlink_start, NULL);
#endif
