/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt.c -- large time stat data extention plugin. */

#include "sdext_lt.h"

static uint16_t sdext_lt_length(void *body) {
	return sizeof(sdext_lt_t);
}

#ifndef ENABLE_STAND_ALONE
static errno_t sdext_lt_open(void *body, 
			     void *hint) 
{
	sdext_lt_t *ext;
	sdext_lt_hint_t *sdext_lt;
    
	aal_assert("umka-1477", body != NULL);
	aal_assert("umka-1478", hint != NULL);

	ext = (sdext_lt_t *)body;
	sdext_lt = (sdext_lt_hint_t *)hint;
    
	sdext_lt->atime = sdext_lt_get_atime(ext);
	sdext_lt->mtime = sdext_lt_get_mtime(ext);
	sdext_lt->ctime = sdext_lt_get_ctime(ext);
    
	return 0;
}

static errno_t sdext_lt_init(void *body, 
			     void *hint)
{
	sdext_lt_hint_t *sdext_lt;
    
	aal_assert("umka-1475", body != NULL);
	aal_assert("umka-1476", hint != NULL);
	
	sdext_lt = (sdext_lt_hint_t *)hint;
    
	sdext_lt_set_atime((sdext_lt_t *)body,
			   sdext_lt->atime);
	
	sdext_lt_set_mtime((sdext_lt_t *)body,
			   sdext_lt->mtime);
	
	sdext_lt_set_ctime((sdext_lt_t *)body,
			   sdext_lt->ctime);

	return 0;
}

static errno_t sdext_lt_print(void *body,
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

extern errno_t sdext_lt_check_struct(sdext_entity_t *sdext,
				     uint8_t mode);
#endif

static reiser4_sdext_ops_t sdext_lt_ops = {
#ifndef ENABLE_STAND_ALONE
	.open	   	= sdext_lt_open,
	.init	   	= sdext_lt_init,
	.print     	= sdext_lt_print,
	.check_struct	= sdext_lt_check_struct,
#else
	.open	   	= NULL,
#endif
	.length	   	= sdext_lt_length
};

static reiser4_plug_t sdext_lt_plug = {
	.cl    = CLASS_INIT,
	.id    = {SDEXT_LT_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_lt",
	.desc  = "Large times stat data extention for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_lt_ops
	}
};

static reiser4_plug_t *sdext_lt_start(reiser4_core_t *c) {
	return &sdext_lt_plug;
}

plug_register(sdext_lt, sdext_lt_start, NULL);
