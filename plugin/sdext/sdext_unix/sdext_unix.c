/*
  sdext_unix.c -- stat data exception plugin, that implements unix stat data
  fields.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ENABLE_COMPACT

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <time.h>
#include <sys/types.h>
#endif

#include <aux/aux.h>
#include "sdext_unix.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_unix_plugin;

static errno_t sdext_unix_init(rbody_t *body, 
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

static errno_t sdext_unix_open(rbody_t *body, 
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

static uint16_t sdext_unix_length(rbody_t *body) {
	return sizeof(sdext_unix_t);
}

#ifndef ENABLE_COMPACT

static errno_t sdext_unix_print(rbody_t *body, aal_stream_t *stream,
				uint16_t options)
{
	sdext_unix_t *ext;
	uint32_t atm, mtm, ctm;
	char uid[255], gid[255];
	
	aal_assert("umka-1412", body != NULL, return -1);
	aal_assert("umka-1413", stream != NULL, return -1);

	ext = (sdext_unix_t *)body;

	aal_memset(uid, 0, sizeof(uid));
	aal_memset(gid, 0, sizeof(gid));

	aal_stream_format(stream, "uid:\t\t%u\n", sdext_unix_get_uid(ext));
	aal_stream_format(stream, "gid:\t\t%u\n", sdext_unix_get_gid(ext));
	
	atm = sdext_unix_get_atime(ext);
	mtm = sdext_unix_get_mtime(ext);
	ctm = sdext_unix_get_ctime(ext);

	aal_stream_format(stream, "atime:\t\t%s", ctime((time_t *)&atm));
	aal_stream_format(stream, "mtime:\t\t%s", ctime((time_t *)&mtm));
	aal_stream_format(stream, "ctime:\t\t%s", ctime((time_t *)&ctm));

	aal_stream_format(stream, "rdev:\t\t%u\n", sdext_unix_get_rdev(ext));
	aal_stream_format(stream, "bytes:\t\t%llu\n", sdext_unix_get_bytes(ext));

	return 0;
}

#endif

static reiser4_plugin_t sdext_unix_plugin = {
	.sdext_ops = {
		.h = {
			.handle = empty_handle,
			.id = SDEXT_UNIX_ID,
			.group = 0,
			.type = SDEXT_PLUGIN_TYPE,
			.label = "sdext_unix",
			.desc = "Unix stat data extention for reiserfs 4.0, ver. " VERSION,
		},
		.init	 = sdext_unix_init,
		.open	 = sdext_unix_open,
		
#ifndef ENABLE_COMPACT		
		.print   = sdext_unix_print,
#else
		.print   = NULL,
#endif
		.length	 = sdext_unix_length
	}
};

static reiser4_plugin_t *sdext_unix_start(reiser4_core_t *c) {
	core = c;
	return &sdext_unix_plugin;
}

plugin_register(sdext_unix_start, NULL);

