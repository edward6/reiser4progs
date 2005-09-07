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
	
	offset = plug_call(key->plug->pl.key, get_offset, key);

	offset += (trans_func ? trans_func(place, pos) : pos);
	
	plug_call(key->plug->pl.key, set_offset, key, offset);
	
	return 0;
}

/* Returns maximal possible key for file body item at @place. */
errno_t body40_maxposs_key(reiser4_place_t *place, reiser4_key_t *key) {
	uint64_t offset;
	reiser4_key_t *maxkey;
   
	aal_memcpy(key, &place->key, sizeof(*key));
    
	maxkey = plug_call(key->plug->pl.key, maximal);
    
	offset = plug_call(key->plug->pl.key, get_offset, maxkey);
	
    	plug_call(key->plug->pl.key, set_offset, key, offset);

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

	units = plug_call(place->plug->pl.item->balance, units, place);

	aal_memcpy(key, &place->key, sizeof(*key));

	offset = plug_call(key->plug->pl.key, get_offset, key);

	offset += (trans_func ? trans_func(place, units) : units);
	
	plug_call(key->plug->pl.key, set_offset, key, offset - 1);
	
	return 0;
}

/* Checks if two file body items are mergeable. */
int body40_mergeable(reiser4_place_t *place1,
		     reiser4_place_t *place2)
{
	uint64_t offset;
	reiser4_key_t maxkey;

	plug_call(place1->plug->pl.item->balance,
		  maxreal_key, place1, &maxkey);

	offset = plug_call(maxkey.plug->pl.key,
			   get_offset, &maxkey);

	plug_call(maxkey.plug->pl.key,
		  set_offset, &maxkey, offset + 1);
	
	return !plug_call(place1->key.plug->pl.key,
			  compfull, &maxkey, &place2->key);
}
#endif
