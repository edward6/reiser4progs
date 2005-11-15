/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   body40.c -- file body item plugins common code. */

#include "body40.h"

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
errno_t body40_get_key(reiser4_place_t *place, uint32_t pos,
		       reiser4_key_t *key,
		       trans_func_t trans_func)
{
	uint64_t offset;

	aal_memcpy(key, &place->key, sizeof(*key));
	
	offset = objcall(key, get_offset);
	offset += (trans_func ? trans_func(place, pos) : pos);
	objcall(key, set_offset, offset);
	
	return 0;
}

/* Returns maximal possible key for file body item at @place. */
errno_t body40_maxposs_key(reiser4_place_t *place, reiser4_key_t *key) {
	uint64_t offset;
	reiser4_key_t *maxkey;
   
	aal_memcpy(key, &place->key, sizeof(*key));
    
	maxkey = plugcall(key->plug, maximal);
	offset = objcall(maxkey, get_offset);
    	objcall(key, set_offset, offset);

	return 0;
}

#ifndef ENABLE_MINIMAL
/* Returns max real key inside passed @place. */
errno_t body40_maxreal_key(reiser4_place_t *place,
			   reiser4_key_t *key,
			   trans_func_t trans_func) 
{
	uint64_t units;
	uint64_t offset;

	units = objcall(place, balance->units);

	aal_memcpy(key, &place->key, sizeof(*key));

	offset = objcall(key, get_offset);
	offset += (trans_func ? trans_func(place, units) : units);	
	objcall(key, set_offset, offset - 1);
	
	return 0;
}

/* Checks if two file body items are mergeable. */
int body40_mergeable(reiser4_place_t *place1,
		     reiser4_place_t *place2)
{
	uint64_t offset;
	reiser4_key_t maxkey;

	objcall(place1, balance->maxreal_key, &maxkey);
	offset = objcall(&maxkey, get_offset);
	objcall(&maxkey, set_offset, offset + 1);
	
	return !objcall(&maxkey, compfull, &place2->key);
}
#endif
