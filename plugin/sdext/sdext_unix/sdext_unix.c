/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_unix.c -- stat data exception plugin, that implements unix 
   stat data fields. */

#include "sdext_unix.h"

static uint16_t sdext_unix_length(void *body, void *hint) {
	return sizeof(sdext_unix_t);
}

#ifndef ENABLE_STAND_ALONE
static errno_t sdext_unix_open(void *body, 
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

static errno_t sdext_unix_init(void *body, 
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

	if (sdext_unix->rdev)
		sdext_unix_set_rdev(ext, sdext_unix->rdev);
	else
		sdext_unix_set_bytes(ext, sdext_unix->bytes);

	return 0;
}

extern errno_t sdext_unix_check_struct(sdext_entity_t *sdext,
				       uint8_t mode);

extern void sdext_unix_print(void *body, aal_stream_t *stream, 
			     uint16_t options);

#endif

static reiser4_sdext_ops_t sdext_unix_ops = {
#ifndef ENABLE_STAND_ALONE
	.open	   	= sdext_unix_open,
	.init	   	= sdext_unix_init,
	.print     	= sdext_unix_print,
	.check_struct	= sdext_unix_check_struct,
#else
	.open	   	= NULL,
#endif
	.length	   	= sdext_unix_length
};

static reiser4_plug_t sdext_unix_plug = {
	.cl    = class_init,
	.id    = {SDEXT_UNIX_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_unix",
	.desc  = "Unix stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_unix_ops
	}
};

static reiser4_plug_t *sdext_unix_start(reiser4_core_t *c) {
	return &sdext_unix_plug;
}

plug_register(sdext_unix, sdext_unix_start, NULL);
