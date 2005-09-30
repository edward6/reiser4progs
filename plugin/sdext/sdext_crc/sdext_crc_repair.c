/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_crc_repair.c -- crypto stat data extention plugin repair code. */

#ifndef ENABLE_MINIMAL

#include "sdext_crc.h"
#include <repair/plugin.h>

void sdext_crc_print(stat_entity_t *stat, 
		     aal_stream_t *stream, 
		     uint16_t options) 
{
	sdext_crc_t *ext;
	uint16_t count, i;
	
	aal_assert("umka-1412", stat != NULL);
	aal_assert("umka-1413", stream != NULL);

	ext = (sdext_crc_t *)stat_body(stat);
	
	aal_stream_format(stream, "key size:\t\t%u\n\t\t", 
			  sdext_crc_get_keylen(ext));

	if (!stat->info.digest) {
		aal_stream_format(stream, "<unknown keyid>\n");
		return;
	}
	
	count = 4 << (uint32_t)stat->info.digest;
	aal_stream_format(stream, "[%u]: ", count);
	
	for (i = 0; i < count; i++)
		aal_stream_format(stream, "%.2x", ext->sign[i]);

	aal_stream_format(stream, "\n");
}

errno_t sdext_crc_check_struct(stat_entity_t *stat, repair_hint_t *hint) {
	uint32_t len;
	
	aal_assert("vpf-1845", stat != NULL);
	aal_assert("vpf-1846", stat->ext_plug != NULL);

	len = sdext_crc_length(stat, NULL);
	
	if (stat->offset + len > stat->place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not look "
			  "like a valid (%s) statdata extension.", 
			  place_blknr(stat->place), stat->place->pos.item,
			  print_key(sdext_crc_core, &stat->place->key), 
			  stat->ext_plug->label);
		
		return RE_FATAL;
	}

	return 0;
}

#endif
