/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key_large_repair.c -- large key plugin repair methods. */

#ifdef ENABLE_LARGE_KEYS
#include "key_large.h"
#include <repair/plugin.h>

extern key_type_t key_large_get_type(key_entity_t *key);
extern void key_large_set_locality(key_entity_t *key, key_type_t type);
extern uint64_t key_large_get_locality(key_entity_t *key);
extern void key_large_set_fobjectid(key_entity_t *key, uint64_t objectid);
extern uint64_t key_large_get_fobjectid(key_entity_t *key);

/* Checks than oid is not used in neither locality not objectid. */
errno_t key_large_check_struct(key_entity_t *key) {
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

#endif
