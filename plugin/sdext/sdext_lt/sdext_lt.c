/*
  sdext_lt.c -- large time stat data extention plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "sdext_lt.h"
#include <aux/aux.h>

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_lt_plugin;

static errno_t sdext_lt_open(body_t *body, 
			     void *hint) 
{
	sdext_lt_t *ext;
	reiser4_sdext_lt_hint_t *sdext_lt;
    
	aal_assert("umka-1477", body != NULL);
	aal_assert("umka-1478", hint != NULL);

	ext = (sdext_lt_t *)body;
	sdext_lt = (reiser4_sdext_lt_hint_t *)hint;
    
	sdext_lt->atime = sdext_lt_get_atime(ext);
	sdext_lt->mtime = sdext_lt_get_mtime(ext);
	sdext_lt->ctime = sdext_lt_get_ctime(ext);
    
	return 0;
}

static uint16_t sdext_lt_length(body_t *body) {
	return sizeof(sdext_lt_t);
}

#ifndef ENABLE_STAND_ALONE

static errno_t sdext_lt_init(body_t *body, 
			     void *hint) 
{
	sdext_lt_t *ext;
	reiser4_sdext_lt_hint_t *sdext_lt;
    
	aal_assert("umka-1475", body != NULL);
	aal_assert("umka-1476", hint != NULL);
	
	ext = (sdext_lt_t *)body;
	sdext_lt = (reiser4_sdext_lt_hint_t *)hint;
    
	sdext_lt_set_atime(ext, sdext_lt->atime);
	sdext_lt_set_mtime(ext, sdext_lt->mtime);
	sdext_lt_set_ctime(ext, sdext_lt->ctime);

	return 0;
}

static errno_t sdext_lt_print(body_t *body,
			      aal_stream_t *stream,
			      uint16_t options)
{
	sdext_lt_t *ext;
	
	aal_assert("umka-1479", body != NULL);
	aal_assert("umka-1480", stream != NULL);

	ext = (sdext_lt_t *)body;

	aal_stream_format(stream, "atime:\t\t%u\n",
			  sdext_lt_get_atime(ext));
	
	aal_stream_format(stream, "mtime:\t\t%u\n",
			  sdext_lt_get_mtime(ext));
	
	aal_stream_format(stream, "ctime:\t\t%u\n",
			  sdext_lt_get_ctime(ext));
	
	return 0;
}

extern errno_t sdext_lt_check(sdext_entity_t *sdext, uint8_t mode);

#endif

static reiser4_plugin_t sdext_lt_plugin = {
	.sdext_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = SDEXT_LT_ID,
			.group = 0,
			.type = SDEXT_PLUGIN_TYPE,
			.label = "sdext_lt",
#ifndef ENABLE_STAND_ALONE
			.desc = "Large times stat data extention for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		.open	 = sdext_lt_open,
		
#ifndef ENABLE_STAND_ALONE
		.init	 = sdext_lt_init,
		.print   = sdext_lt_print,
		.check   = sdext_lt_check,
#endif		
		.length	 = sdext_lt_length
	}
};

static reiser4_plugin_t *sdext_lt_start(reiser4_core_t *c) {
	core = c;
	return &sdext_lt_plugin;
}

plugin_register(sdext_lt_start, NULL);

