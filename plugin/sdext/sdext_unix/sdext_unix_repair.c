/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_unix.c -- stat data exception plugin, that implements unix
   stat data fields. */

#ifndef ENABLE_STAND_ALONE

#include <time.h>
#include <sys/types.h>
#include "sdext_unix.h"
#include <repair/plugin.h>

errno_t sdext_unix_check_struct(stat_entity_t *stat, repair_hint_t *hint) {
	aal_assert("vpf-778", stat != NULL);
	aal_assert("vpf-781", stat->ext_plug != NULL);
	
	if (stat->offset + sizeof(sdext_unix_t) > stat->place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not look "
			  "like a valid (%s) statdata extension.", 
			  place_blknr(stat->place), stat->place->pos.item,
			  print_key(sdext_unix_core, &stat->place->key), 
			  stat->ext_plug->label);
		
		return RE_FATAL;
	}
	
	return 0;
}

void sdext_unix_print(stat_entity_t *stat, aal_stream_t *stream, uint16_t options) {
	sdext_unix_t *ext;
	time_t atm, mtm, ctm;
	char uid[255], gid[255];
	
	aal_assert("umka-1412", stat != NULL);
	aal_assert("umka-1413", stream != NULL);

	ext = (sdext_unix_t *)stat_body(stat);

	aal_memset(uid, 0, sizeof(uid));
	aal_memset(gid, 0, sizeof(gid));

	aal_stream_format(stream, "uid:\t\t%u\n",
			  sdext_unix_get_uid(ext));
	
	aal_stream_format(stream, "gid:\t\t%u\n",
			  sdext_unix_get_gid(ext));
	
	atm = sdext_unix_get_atime(ext);
	mtm = sdext_unix_get_mtime(ext);
	ctm = sdext_unix_get_ctime(ext);

	aal_stream_format(stream, "atime:\t\t%s",
			  ctime(&atm));
	
	aal_stream_format(stream, "mtime:\t\t%s",
			  ctime(&mtm));
	
	aal_stream_format(stream, "ctime:\t\t%s",
			  ctime(&ctm));

	aal_stream_format(stream, "rdev:\t\t%llu\n",
			  sdext_unix_get_rdev(ext));
	
	aal_stream_format(stream, "bytes:\t\t%llu\n",
			  sdext_unix_get_bytes(ext));
}

#endif
