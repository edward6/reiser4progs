/*
  sdext_lw.c -- light weight stat data extention plugin, that implements base
  stat data fields.
    
  Copyright 1996-2002 (C) Hans Reiser.
*/

#include "sdext_lw.h"

static reiser4_core_t *core = NULL;

static errno_t sdext_lw_init(reiser4_body_t *body, 
			     void *hint) 
{
	sdext_lw_t *ext;
	reiser4_sdext_lw_hint_t *sdext_lw;
    
	aal_assert("umka-1186", body != NULL, return -1);
	aal_assert("umka-1187", hint != NULL, return -1);
	
	ext = (sdext_lw_t *)body;
	sdext_lw = (reiser4_sdext_lw_hint_t *)hint;
    
	sdext_lw_set_mode(ext, sdext_lw->mode);
	sdext_lw_set_nlink(ext, sdext_lw->nlink);
	sdext_lw_set_size(ext, sdext_lw->size);

	return 0;
}

static errno_t sdext_lw_open(reiser4_body_t *body, 
			     void *hint) 
{
	sdext_lw_t *ext;
	reiser4_sdext_lw_hint_t *sdext_lw;
    
	aal_assert("umka-1188", body != NULL, return -1);
	aal_assert("umka-1189", hint != NULL, return -1);

	ext = (sdext_lw_t *)body;
	sdext_lw = (reiser4_sdext_lw_hint_t *)hint;
    
	sdext_lw->mode = sdext_lw_get_mode(ext);
	sdext_lw->nlink = sdext_lw_get_nlink(ext);
	sdext_lw->size = sdext_lw_get_size(ext);
    
	return 0;
}

static uint16_t sdext_lw_length(void) {
	return sizeof(sdext_lw_t);
}

static errno_t sdext_lw_print(reiser4_body_t *body,
			      char *buff, uint32_t n,
			      uint16_t options)
{
	aal_assert("umka-1410", body != NULL, return -1);
	aal_assert("umka-1411", buff != NULL, return -1);

	return 0;
}

static reiser4_plugin_t sdext_lw_plugin = {
	.sdext_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = SDEXT_LW_ID,
				.group = 0,
				.type = SDEXT_PLUGIN_TYPE
			},
			.label = "sdext_lw",
			.desc = "Base stat data extention for reiserfs 4.0, ver. " VERSION,
		},
		.init	 = sdext_lw_init,
		.open	 = sdext_lw_open,
		.print   = sdext_lw_print,
		.length	 = sdext_lw_length
	}
};

static reiser4_plugin_t *sdext_lw_start(reiser4_core_t *c) {
	core = c;
	return &sdext_lw_plugin;
}

plugin_register(sdext_lw_start, NULL);

