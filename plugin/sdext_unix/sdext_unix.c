/*
  sdext_unix.c -- stat data exception plugin, that implements unix stat data
  fields.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#ifndef ENABLE_COMPACT

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <pwd.h>
#include <time.h>
#include <sys/types.h>
#endif

#include <aux/aux.h>
#include "sdext_unix.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_unix_plugin;

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

static char *sdext_unix_name(char *pwdline) {
	return aal_strsep(&pwdline, ":");
}

static char *sdext_unix_gecos(char *pwdline) {
	char *token;
	int counter = 0;

	while ((token = aal_strsep(&pwdline, ":")))
		if (counter++ == 4) return token;
	
	return NULL;
}

#ifndef ENABLE_COMPACT

static errno_t sdext_unix_print(reiser4_body_t *body,
				char *buff, uint32_t n,
				uint16_t options)
{
	sdext_unix_t *ext;
	uint32_t atm, mtm, ctm;
	char uid[255], gid[255];
	
	aal_assert("umka-1412", body != NULL, return -1);
	aal_assert("umka-1413", buff != NULL, return -1);

	ext = (sdext_unix_t *)body;

	aal_memset(uid, 0, sizeof(uid));
	aal_memset(gid, 0, sizeof(gid));

	if (!getpw(sdext_unix_get_uid(ext), uid) && !getpw(sdext_unix_get_uid(ext), gid)) {
		aux_strncat(buff, n, "uid:\t\t%s (%s)\n", sdext_unix_name(uid),
			    sdext_unix_gecos(uid));
		aux_strncat(buff, n, "gid:\t\t%s (%s)\n", sdext_unix_name(gid),
			    sdext_unix_gecos(gid));
	} else {
		aux_strncat(buff, n, "uid:\t\t%u\n", sdext_unix_get_uid(ext));
		aux_strncat(buff, n, "gid:\t\t%u\n", sdext_unix_get_gid(ext));
	}
	
	atm = sdext_unix_get_atime(ext);
	mtm = sdext_unix_get_mtime(ext);
	ctm = sdext_unix_get_ctime(ext);

	aux_strncat(buff, n, "atime:\t\t%s", ctime((time_t *)&atm));
	aux_strncat(buff, n, "mtime:\t\t%s", ctime((time_t *)&mtm));
	aux_strncat(buff, n, "ctime:\t\t%s", ctime((time_t *)&ctm));

	aux_strncat(buff, n, "rdev:\t\t%u\n", sdext_unix_get_rdev(ext));
	aux_strncat(buff, n, "bytes:\t\t%llu\n", sdext_unix_get_bytes(ext));

	return 0;
}

#endif

static reiser4_plugin_t sdext_unix_plugin = {
	.sdext_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = SDEXT_UNIX_ID,
				.group = 0,
				.type = SDEXT_PLUGIN_TYPE
			},
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

