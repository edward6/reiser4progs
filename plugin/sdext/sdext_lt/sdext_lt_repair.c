/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt_repair.c -- large time stat data extension plugin recovery code. */

#ifndef ENABLE_STAND_ALONE
#include "sdext_lt.h"
#include <repair/plugin.h>

errno_t sdext_lt_check_struct(stat_entity_t *stat, uint8_t mode) {
	aal_assert("vpf-776", stat != NULL);
	aal_assert("vpf-782", stat->ext_plug != NULL);
	
	if (stat->offset + sizeof(sdext_lt_t) > stat->place->len) {
		aal_error("Does not look like a valid (%s) statdata "
			  "extension.", stat->ext_plug->label);
		return RE_FATAL;
	}
	
	return 0;
}

/* Prints extension into passed @stream. */
void sdext_lt_print(stat_entity_t *stat, 
		    aal_stream_t *stream, 
		    uint16_t options) 
{
	sdext_lt_t *ext;
	
	aal_assert("umka-1479", stat != NULL);
	aal_assert("umka-1480", stream != NULL);

	ext = (sdext_lt_t *)stat_body(stat);

	aal_stream_format(stream, "atime:\t\t%u\n",
			  sdext_lt_get_atime(ext));
	
	aal_stream_format(stream, "mtime:\t\t%u\n",
			  sdext_lt_get_mtime(ext));
	
	aal_stream_format(stream, "ctime:\t\t%u\n",
			  sdext_lt_get_ctime(ext));
}

#endif
