/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lw_repair.c -- light weight stat data extension plugin recovery 
   code. */

#ifndef ENABLE_STAND_ALONE
#include <sys/stat.h>
#include "sdext_lw.h"
#include <repair/plugin.h>

errno_t sdext_lw_check_struct(sdext_entity_t *sdext, uint8_t mode) {
	aal_assert("vpf-777", sdext != NULL);
	aal_assert("vpf-783", sdext->plug != NULL);
	
	if (sdext->offset + sizeof(sdext_lw_t) > sdext->sdlen) {
		aal_error("Does not look like a valid (%s) statdata "
			  "extension.", sdext->plug->label);
		return 	RE_FATAL;
	}
	
	return 0;
}

/* Mode parse stuff. */
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
	if (S_ISREG (mode))
		return '-';

	return '?';
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

/* Print extension to passed @stream. */
errno_t sdext_lw_print(void *body, aal_stream_t *stream,
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
#endif

