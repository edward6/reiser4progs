/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_short_repair.c -- short key plugin repair methods. */

#ifdef ENABLE_SHORT_KEYS
#include "key_short.h"
#include <repair/repair.h>

extern key_type_t key_short_get_type(key_entity_t *key);
extern void key_short_set_locality(key_entity_t *key, key_type_t type);
extern uint64_t key_short_get_locality(key_entity_t *key);
extern void key_short_set_fobjectid(key_entity_t *key, uint64_t objectid);
extern uint64_t key_short_get_fobjectid(key_entity_t *key);
	
/* Checks than oid is not used in neither locality not objectid. */
errno_t key_short_check_struct(key_entity_t *key) {
	key_minor_t minor;
	uint64_t oid;
	
	aal_assert("vpf-1278", key != NULL);

	minor = ks_get_minor((key_short_t *)key->body); 
	
	if (minor >= KEY_LAST_MINOR)
		return RE_FATAL;

	oid = key_short_get_locality(key);

	if (oid & KEY_SHORT_BAND_MASK)
		key_short_set_locality(key, oid & !KEY_SHORT_BAND_MASK);
	
	/* Direntries needs locality only to be checked. */
	if (key_short_get_type(key) == KEY_FILENAME_TYPE)
		return 0;
	
	oid = key_short_get_fobjectid(key);

	if (oid & KEY_SHORT_BAND_MASK)
		key_short_set_fobjectid(key, oid & !KEY_SHORT_BAND_MASK);
	
	return 0;
}

#endif
