/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_large_repair.c -- large key plugin repair methods. */

#ifdef ENABLE_LARGE_KEYS
#include "key_large.h"
#include <repair/plugin.h>

/* Checks than oid is not used in neither locality not objectid. */
errno_t key_large_check_struct(reiser4_key_t *key) {
	key_minor_t minor;
	uint64_t oid;
	
	aal_assert("vpf-1278", key != NULL);
	
	minor = kl_get_minor((key_large_t *)key->body); 
	
	if (minor >= KEY_LAST_MINOR)
		return RE_FATAL;
	
	oid = key_large_get_locality(key);

	if (oid & KEY_LARGE_BAND_MASK)
		key_large_set_locality(key, oid & !KEY_LARGE_BAND_MASK);
	
	/* Direntries needs locality only to be checked. */
	if (key_large_get_type(key) == KEY_FILENAME_TYPE)
		return 0;
	
	oid = key_large_get_fobjectid(key);

	if (oid & KEY_LARGE_BAND_MASK)
		key_large_set_fobjectid(key, oid & !KEY_LARGE_BAND_MASK);
	
	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Prints key into passed stream */
void key_large_print(reiser4_key_t *key, aal_stream_t *stream,
		     uint16_t options)
{
	const char *name;
	
	aal_assert("vpf-191", key != NULL);
	aal_assert("umka-1548", stream != NULL);

	if (options == PO_INODE) {
		aal_stream_format(stream, "%llx:%llx:%llx\0",
				  key_large_get_locality(key),
				  key_large_get_ordering(key),
				  key_large_get_objectid(key));
	} else {
		name = key_common_minor2name(key_large_get_type(key));

		aal_stream_format(stream, "%llx:%x(%s):%llx:%llx:%llx\0",
				  key_large_get_locality(key),
				  key_large_get_type(key), name,
				  key_large_get_ordering(key),
				  key_large_get_fobjectid(key),
				  key_large_get_offset(key));
	}
}
#endif

#endif
