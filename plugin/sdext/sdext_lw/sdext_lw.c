/*
  sdext_lw.c -- light weight stat data extention plugin, that implements base
  stat data fields.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "sdext_lw.h"
#include <aux/aux.h>

#ifndef ENABLE_STAND_ALONE
#  include <sys/stat.h>
#endif

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sdext_lw_plugin;

static errno_t sdext_lw_open(body_t *body, 
			     void *hint) 
{
	sdext_lw_t *ext;
	sdext_lw_hint_t *sdext_lw;
    
	aal_assert("umka-1188", body != NULL);
	aal_assert("umka-1189", hint != NULL);

	ext = (sdext_lw_t *)body;
	sdext_lw = (sdext_lw_hint_t *)hint;
    
	sdext_lw->mode = sdext_lw_get_mode(ext);
	sdext_lw->nlink = sdext_lw_get_nlink(ext);
	sdext_lw->size = sdext_lw_get_size(ext);
    
	return 0;
}

static uint16_t sdext_lw_length(body_t *body) {
	return sizeof(sdext_lw_t);
}

#ifndef ENABLE_STAND_ALONE

static errno_t sdext_lw_init(body_t *body, 
			     void *hint) 
{
	sdext_lw_hint_t *sdext_lw;
    
	aal_assert("umka-1186", body != NULL);
	aal_assert("umka-1187", hint != NULL);
	
	sdext_lw = (sdext_lw_hint_t *)hint;
    
	sdext_lw_set_mode((sdext_lw_t *)body,
			  sdext_lw->mode);
	
	sdext_lw_set_nlink((sdext_lw_t *)body,
			   sdext_lw->nlink);
	
	sdext_lw_set_size((sdext_lw_t *)body,
			  sdext_lw->size);

	return 0;
}

static char sdext_lw_file_type(uint16_t mode) {
	if (S_ISDIR(mode))
		return 'd';

	if (S_ISCHR(mode))
		return 'c';

	if (S_ISBLK(mode))
		return 'b';

	if (S_ISFIFO(mode))
		return 'p';

	if (S_ISLNK(mode))
		return 'l';

	if (S_ISSOCK(mode))
		return 's';

	return '-';
}

static void sdext_lw_parse_mode(uint16_t mode, char *str) {
	str[0] = sdext_lw_file_type(mode);
	str[1] = mode & S_IRUSR ? 'r' : '-';
	str[2] = mode & S_IWUSR ? 'w' : '-';
	str[3] = mode & S_IXUSR ? 'x' : '-';
	str[4] = mode & S_IRGRP ? 'r' : '-';
	str[5] = mode & S_IWGRP ? 'w' : '-';
	str[6] = mode & S_IXGRP ? 'x' : '-';
	str[7] = mode & S_IROTH ? 'r' : '-';
	str[8] = mode & S_IWOTH ? 'w' : '-';
	str[9] = mode & S_IXOTH ? 'x' : '-';
	str[10] = '\0';
}

static errno_t sdext_lw_print(body_t *body, aal_stream_t *stream,
			      uint16_t options)
{
	char mode[16];
	sdext_lw_t *ext;
	
	aal_assert("umka-1410", body != NULL);
	aal_assert("umka-1411", stream != NULL);

	ext = (sdext_lw_t *)body;

	aal_memset(mode, 0, sizeof(mode));

	sdext_lw_parse_mode(sdext_lw_get_mode(ext), mode);
	
	aal_stream_format(stream, "mode:\t\t%s\n", mode);
	
	aal_stream_format(stream, "nlink:\t\t%u\n",
			  sdext_lw_get_nlink(ext));
	
	aal_stream_format(stream, "size:\t\t%llu\n",
			  sdext_lw_get_size(ext));
	
	return 0;
}

extern errno_t sdext_lw_check(sdext_entity_t *sdext,
			      uint8_t mode);

#endif

static reiser4_plugin_t sdext_lw_plugin = {
	.sdext_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = SDEXT_LW_ID,
			.group = 0,
			.type = SDEXT_PLUGIN_TYPE,
			.label = "sdext_lw",
#ifndef ENABLE_STAND_ALONE
			.desc = "Light stat data extention for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		.open	 = sdext_lw_open,
		
#ifndef ENABLE_STAND_ALONE
		.init	 = sdext_lw_init,
		.print   = sdext_lw_print,
		.check   = sdext_lw_check,
#endif		
		.length	 = sdext_lw_length
	}
};

static reiser4_plugin_t *sdext_lw_start(reiser4_core_t *c) {
	core = c;
	return &sdext_lw_plugin;
}

plugin_register(sdext_lw, sdext_lw_start, NULL);

