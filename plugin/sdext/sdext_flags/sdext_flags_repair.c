/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_flags_repair.c -- inode flags stat data extension plugin recovery
   code. */

#ifndef ENABLE_MINIMAL
#include "sdext_flags.h"
#include <repair/plugin.h>

#include "sdext_flags.h"

errno_t sdext_flags_check_struct(stat_entity_t *stat, repair_hint_t *hint) {
	aal_assert("umka-3081", stat != NULL);
	aal_assert("umka-3082", stat->ext_plug != NULL);
	
	if (stat->offset + sizeof(sdext_flags_t) > stat->place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not look "
			  "like a valid (%s) statdata extension.", 
			  place_blknr(stat->place), stat->place->pos.item,
			  print_key(sdext_flags_core, &stat->place->key),
			  stat->ext_plug->label);
		return RE_FATAL;
	}
	
	return 0;
}

/* Prints extension into passed @stream. */
void sdext_flags_print(stat_entity_t *stat, 
		       aal_stream_t *stream,
		       uint16_t options)
{
	sdext_flags_t *ext;
	
	aal_assert("umka-3083", stat != NULL);
	aal_assert("umka-3084", stream != NULL);

	ext = (sdext_flags_t *)stat_body(stat);

	if (sizeof(sdext_flags_t) + stat->offset > stat->place->len) {
		aal_stream_format(stream, "No enough space (%u bytes) "
				  "for the flags extention body.\n", 
				  stat->place->len - stat->offset);
		return;
	}
	
	aal_stream_format(stream, "flags:\t\t%u\n",
			  sdext_flags_get_flags(ext));
}

#endif
