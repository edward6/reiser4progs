/*
  sdext_symlink.c -- symlink stat data extention plugin.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aux/aux.h>
#include <reiser4/plugin.h>

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_symlink_plugin;

static errno_t sdext_symlink_init(reiser4_body_t *body, 
				  void *hint) 
{
	char *data;
	
	aal_assert("umka-1481", body != NULL, return -1);
	aal_assert("umka-1482", hint != NULL, return -1);

	data = (char *)hint;
	aal_memcpy((char *)body, data, aal_strlen(data));
	return 0;
}

static errno_t sdext_symlink_open(reiser4_body_t *body, 
				  void *hint) 
{
	char *data;
	
	aal_assert("umka-1483", body != NULL, return -1);
	aal_assert("umka-1484", hint != NULL, return -1);

	data = (char *)body;
	
	aal_memcpy((char *)hint, data, aal_strlen(data));
	return 0;
}

static uint16_t sdext_symlink_length(reiser4_body_t *body) {
	aal_assert("umka-1488", body != NULL, return 0);
	return aal_strlen((char *)body);
}

#ifndef ENABLE_COMPACT

static errno_t sdext_symlink_print(reiser4_body_t *body,
			      char *buff, uint32_t n,
			      uint16_t options)
{
	aal_assert("umka-1485", body != NULL, return -1);
	aal_assert("umka-1486", buff != NULL, return -1);

	aux_strncat(buff, n, "len:\t\t%u\n", aal_strlen((char *)body));
	aux_strncat(buff, n, "value:\t\t\"%s\"\n", (char *)body);
	return 0;
}

#endif

static reiser4_plugin_t sdext_symlink_plugin = {
	.sdext_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = SDEXT_SYMLINK_ID,
				.group = 0,
				.type = SDEXT_PLUGIN_TYPE
			},
			.label = "sdext_symlink",
			.desc = "Symlink data extention for reiserfs 4.0, ver. " VERSION,
		},
		.init	 = sdext_symlink_init,
		.open	 = sdext_symlink_open,
		
#ifndef ENABLE_COMPACT
		.print   = sdext_symlink_print,
#else
		.print   = NULL,
#endif		
		.length	 = sdext_symlink_length
	}
};

static reiser4_plugin_t *sdext_symlink_start(reiser4_core_t *c) {
	core = c;
	return &sdext_symlink_plugin;
}

plugin_register(sdext_symlink_start, NULL);

