/*
  sdext_unix.c -- stat data exception plugin, that implements unix stat data
  fields.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ENABLE_STAND_ALONE

#include <time.h>
#include <sys/types.h>

#include <aux/aux.h>
#include "sdext_unix.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_unix_plugin;

static errno_t sdext_unix_open(body_t *body, 
			       void *hint) 
{
	sdext_unix_t *ext;
	sdext_unix_hint_t *sdext_unix;
    
	aal_assert("umka-886", body != NULL);
	aal_assert("umka-887", hint != NULL);

	ext = (sdext_unix_t *)body;
	sdext_unix = (sdext_unix_hint_t *)hint;
    
	sdext_unix->uid = sdext_unix_get_uid(ext);
	sdext_unix->gid = sdext_unix_get_gid(ext);
	sdext_unix->atime = sdext_unix_get_atime(ext);
	sdext_unix->mtime = sdext_unix_get_mtime(ext);
	sdext_unix->ctime = sdext_unix_get_ctime(ext);
	sdext_unix->rdev = sdext_unix_get_rdev(ext);
	sdext_unix->bytes = sdext_unix_get_bytes(ext);

	return 0;
}

static uint16_t sdext_unix_length(body_t *body) {
	return sizeof(sdext_unix_t);
}

static errno_t sdext_unix_init(body_t *body, 
			       void *hint) 
{
	sdext_unix_t *ext;
	sdext_unix_hint_t *sdext_unix;
    
	aal_assert("umka-884", body != NULL);
	aal_assert("umka-885", hint != NULL);
	
	ext = (sdext_unix_t *)body;
	sdext_unix = (sdext_unix_hint_t *)hint;
    
	sdext_unix_set_uid(ext, sdext_unix->uid);
	sdext_unix_set_gid(ext, sdext_unix->gid);
	sdext_unix_set_atime(ext, sdext_unix->atime);
	sdext_unix_set_mtime(ext, sdext_unix->mtime);
	sdext_unix_set_ctime(ext, sdext_unix->ctime);
	sdext_unix_set_rdev(ext, sdext_unix->rdev);
	sdext_unix_set_bytes(ext, sdext_unix->bytes);

	return 0;
}

static errno_t sdext_unix_print(body_t *body, aal_stream_t *stream,
				uint16_t options)
{
	sdext_unix_t *ext;
	time_t atm, mtm, ctm;
	char uid[255], gid[255];
	
	aal_assert("umka-1412", body != NULL);
	aal_assert("umka-1413", stream != NULL);

	ext = (sdext_unix_t *)body;

	aal_memset(uid, 0, sizeof(uid));
	aal_memset(gid, 0, sizeof(gid));

	aal_stream_format(stream, "uid:\t\t%u\n",
			  sdext_unix_get_uid(ext));
	
	aal_stream_format(stream, "gid:\t\t%u\n",
			  sdext_unix_get_gid(ext));
	
	atm = sdext_unix_get_atime(ext);
	mtm = sdext_unix_get_mtime(ext);
	ctm = sdext_unix_get_ctime(ext);

	aal_stream_format(stream, "atime:\t\t%s",
			  ctime(&atm));
	
	aal_stream_format(stream, "mtime:\t\t%s",
			  ctime(&mtm));
	
	aal_stream_format(stream, "ctime:\t\t%s",
			  ctime(&ctm));

	aal_stream_format(stream, "rdev:\t\t%u\n",
			  sdext_unix_get_rdev(ext));
	
	aal_stream_format(stream, "bytes:\t\t%llu\n",
			  sdext_unix_get_bytes(ext));

	return 0;
}

extern errno_t sdext_unix_check(sdext_entity_t *sdext,
				uint8_t mode);

static reiser4_sdext_ops_t sdext_unix_ops = {
	.open	   = sdext_unix_open,
	.init	   = sdext_unix_init,
	.print     = sdext_unix_print,
	.check     = sdext_unix_check,
	.length	   = sdext_unix_length
};

static reiser4_plugin_t sdext_unix_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = SDEXT_UNIX_ID,
		.group = 0,
		.type = SDEXT_PLUGIN_TYPE,
		.label = "sdext_unix",
		.desc = "Unix stat data extention for "
		"reiser4, ver. " VERSION
	},
	.o = {
		.sdext_ops = &sdext_unix_ops
	}
};

static reiser4_plugin_t *sdext_unix_start(reiser4_core_t *c) {
	core = c;
	return &sdext_unix_plugin;
}

plugin_register(sdext_unix, sdext_unix_start, NULL);
#endif
