/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_symlink.c -- symlink stat data extension plugin. */


#include <reiser4/plugin.h>
#include <repair/plugin.h>

#ifndef ENABLE_STAND_ALONE
#ifdef ENABLE_SYMLINKS
errno_t sdext_symlink_check_struct(stat_entity_t *stat, uint8_t mode) {
	uint32_t offset;
	
	aal_assert("vpf-779", stat != NULL);
	aal_assert("vpf-780", stat->ext_plug != NULL);
	
	offset = stat->offset;
	
	while (*((char *)(stat->place->body) + offset)) {
		offset++;
	}
	
	if (offset >= stat->place->len) {
		aal_error("Does not look like a valid (%s) "
			  "statdata extension.", stat->ext_plug->label);

		return RE_FATAL;
	}
	
	return 0;
}

void sdext_symlink_print(stat_entity_t *stat, 
			 aal_stream_t *stream, 
			 uint16_t options) 
{
	aal_assert("umka-1485", stat != NULL);
	aal_assert("umka-1486", stream != NULL);

	aal_stream_format(stream, "len:\t\t%u\n",
			  aal_strlen((char *)stat_body(stat)));
	
	aal_stream_format(stream, "data:\t\t\"%s\"\n",
			  (char *)stat_body(stat));
}

#endif
#endif

