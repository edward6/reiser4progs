/*
  sdext_symlink.c -- symlink stat data extention plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef ENABLE_SYMLINKS_SUPPORT

#include <aux/aux.h>
#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_symlink_plugin;

static errno_t sdext_symlink_open(body_t *body, 
				  void *hint) 
{
	char *data;
	
	aal_assert("umka-1483", body != NULL);
	aal_assert("umka-1484", hint != NULL);

	data = (char *)body;
	
	aal_memcpy((char *)hint, data, aal_strlen(data));
	return 0;
}

static uint16_t sdext_symlink_length(body_t *body) {
	aal_assert("umka-1488", body != NULL);
	return aal_strlen((char *)body) + 1;
}

#ifndef ENABLE_STAND_ALONE

static errno_t sdext_symlink_init(body_t *body, 
				  void *hint)
{
	char *data;
	
	aal_assert("umka-1481", body != NULL);
	aal_assert("umka-1482", hint != NULL);

	data = (char *)hint;
	aal_memcpy((char *)body, data, aal_strlen(data));
	return 0;
}

static errno_t sdext_symlink_print(body_t *body,
				   aal_stream_t *stream,
				   uint16_t options)
{
	aal_assert("umka-1485", body != NULL);
	aal_assert("umka-1486", stream != NULL);

	aal_stream_format(stream, "len:\t\t%u\n", aal_strlen((char *)body));
	aal_stream_format(stream, "value:\t\t\"%s\"\n", (char *)body);
	
	return 0;
}

extern errno_t sdext_symlink_check(sdext_entity_t *sdext, uint8_t mode);

#endif

static reiser4_plugin_t sdext_symlink_plugin = {
	.sdext_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = SDEXT_SYMLINK_ID,
			.group = 0,
			.type = SDEXT_PLUGIN_TYPE,
			.label = "sdext_symlink",
#ifndef ENABLE_STAND_ALONE
			.desc = "Symlink stat data extention for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		.open	 = sdext_symlink_open,
		
#ifndef ENABLE_STAND_ALONE
		.init	 = sdext_symlink_init,
		.print   = sdext_symlink_print,
		.check   = sdext_symlink_check,
#endif		
		.length	 = sdext_symlink_length
	}
};

static reiser4_plugin_t *sdext_symlink_start(reiser4_core_t *c) {
	core = c;
	return &sdext_symlink_plugin;
}

plugin_register(sdext_symlink, sdext_symlink_start, NULL);

#endif
