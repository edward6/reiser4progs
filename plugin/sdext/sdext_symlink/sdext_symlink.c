/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_symlink.c -- symlink stat data extention plugin. */

#ifdef ENABLE_SYMLINKS
#include <reiser4/plugin.h>

static errno_t sdext_symlink_open(body_t *body, 
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

static uint16_t sdext_symlink_length(body_t *body) {
	aal_assert("umka-1488", body != NULL);
	return aal_strlen((char *)body) + 1;
}

#ifndef ENABLE_STAND_ALONE
static errno_t sdext_symlink_init(body_t *body, 
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

static errno_t sdext_symlink_print(body_t *body,
				   aal_stream_t *stream,
				   uint16_t options)
{
	aal_assert("umka-1485", body != NULL);
	aal_assert("umka-1486", stream != NULL);

	aal_stream_format(stream, "len:\t\t%u\n",
			  aal_strlen((char *)body));
	
	aal_stream_format(stream, "data:\t\t\"%s\"\n",
			  (char *)body);
	
	return 0;
}

extern errno_t sdext_symlink_check_struct(sdext_entity_t *sdext, uint8_t mode);

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

static reiser4_plugin_t sdext_symlink_plugin = {
	.cl    = CLASS_INIT,
	.id    = {SDEXT_SYMLINK_ID, 0, SDEXT_PLUGIN_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_symlink",
	.desc  = "Symlink stat data extention for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_symlink_ops
	}
};

static reiser4_plugin_t *sdext_symlink_start(reiser4_core_t *c) {
	return &sdext_symlink_plugin;
}

plugin_register(sdext_symlink, sdext_symlink_start, NULL);
#endif
