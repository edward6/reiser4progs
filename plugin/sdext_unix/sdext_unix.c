/*
  sdext_unix.c -- stat data exception plugin, that implements unix stat data 
  fields.
  Copyright 1996-2002 (C) Hans Reiser.
*/

#include "sdext_unix.h"

static reiser4_core_t *core = NULL;

static errno_t sdext_unix_init(reiser4_body_t *body, 
							   void *hint) 
{
    sdext_unix_t *ext;
    reiser4_sdext_unix_hint_t *sdext_unix;
    
    aal_assert("umka-884", body != NULL, return -1);
    aal_assert("umka-885", hint != NULL, return -1);
	
    ext = (sdext_unix_t *)body;
    sdext_unix = (reiser4_sdext_unix_hint_t *)hint;
    
    sdext_unix_set_uid(ext, sdext_unix->uid);
    sdext_unix_set_gid(ext, sdext_unix->gid);
    sdext_unix_set_atime(ext, sdext_unix->atime);
    sdext_unix_set_mtime(ext, sdext_unix->mtime);
    sdext_unix_set_ctime(ext, sdext_unix->ctime);
    sdext_unix_set_rdev(ext, sdext_unix->rdev);
    sdext_unix_set_bytes(ext, sdext_unix->bytes);

    return 0;
}

static errno_t sdext_unix_open(reiser4_body_t *body, 
							   void *hint) 
{
    sdext_unix_t *ext;
    reiser4_sdext_unix_hint_t *sdext_unix;
    
    aal_assert("umka-886", body != NULL, return -1);
    aal_assert("umka-887", hint != NULL, return -1);

    ext = (sdext_unix_t *)body;
    sdext_unix = (reiser4_sdext_unix_hint_t *)hint;
    
    sdext_unix->uid = sdext_unix_get_uid(ext);
    sdext_unix->gid = sdext_unix_get_gid(ext);
    sdext_unix->atime = sdext_unix_get_atime(ext);
    sdext_unix->mtime = sdext_unix_get_mtime(ext);
    sdext_unix->ctime = sdext_unix_get_ctime(ext);
    sdext_unix->rdev = sdext_unix_get_rdev(ext);
    sdext_unix->bytes = sdext_unix_get_bytes(ext);

    return 0;
}

static uint16_t sdext_unix_length(void) {
    return sizeof(sdext_unix_t);
}

static errno_t sdext_unix_print(reiser4_body_t *body,
							  char *buff, uint32_t n,
							  uint16_t options)
{
	aal_assert("umka-1412", body != NULL, return -1);
	aal_assert("umka-1413", buff != NULL, return -1);

	return 0;
}

static reiser4_plugin_t sdext_unix_plugin = {
    .sdext_ops = {
		.h = {
			.handle = NULL,
			.id = SDEXT_UNIX_ID,
			.group = 0,
			.type = SDEXT_PLUGIN_TYPE,
			.label = "sdext_unix",
			.desc = "Unix stat data extention for reiserfs 4.0, ver. " VERSION,
		},
		.init	 = sdext_unix_init,
		.open	 = sdext_unix_open,
		.print   = sdext_unix_print,
		.length	 = sdext_unix_length
    }
};

static reiser4_plugin_t *sdext_unix_start(reiser4_core_t *c) {
    core = c;
    return &sdext_unix_plugin;
}

plugin_register(sdext_unix_start);

