/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   body40.c -- file body item plugins common code. */

#include "body40.h"

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
errno_t body40_get_key(place_t *place, uint32_t pos,
		       key_entity_t *key,
		       trans_func_t trans_func)
{
	uint64_t offset;

	plug_call(place->key.plug->o.key_ops, assign,
		  key, &place->key);
		
	offset = plug_call(key->plug->o.key_ops,
			   get_offset, key);

	offset += (trans_func ?
		    trans_func(place, pos) : pos);
	
	plug_call(key->plug->o.key_ops, set_offset,
		  key, offset);
	
	return 0;
}

/* Returns maximal possible key for file body item at @place. */
errno_t body40_maxposs_key(place_t *place, key_entity_t *key) {
	uint64_t offset;
	key_entity_t *maxkey;
    
	plug_call(place->key.plug->o.key_ops, assign,
		  key, &place->key);
    
	maxkey = plug_call(key->plug->o.key_ops,
			   maximal);
    
	offset = plug_call(key->plug->o.key_ops,
			   get_offset, maxkey);
	
    	plug_call(key->plug->o.key_ops, set_offset,
		  key, offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Returns max real key inside passed @place. */
errno_t body40_maxreal_key(place_t *place,
			   key_entity_t *key,
			   trans_func_t trans_func) 
{
	uint64_t units;
	uint64_t offset;

	units = plug_call(place->plug->o.item_ops->balance,
			  units, place);
	
	plug_call(place->key.plug->o.key_ops, assign, key,
		  &place->key);

	offset = plug_call(key->plug->o.key_ops, get_offset,
			   key);

	offset += (trans_func ? trans_func(place, units) :
		   units);
	
	plug_call(key->plug->o.key_ops, set_offset,
		  key, offset - 1);
	
	return 0;
}

/* Checks if two file body items are mergeable. */
int body40_mergeable(place_t *place1, place_t *place2) {
	uint64_t offset;
	key_entity_t maxkey;

	plug_call(place1->plug->o.item_ops->balance,
		  maxreal_key, place1, &maxkey);

	offset = plug_call(maxkey.plug->o.key_ops,
			   get_offset, &maxkey);

	plug_call(maxkey.plug->o.key_ops,
		  set_offset, &maxkey, offset + 1);
	
	return !plug_call(place1->key.plug->o.key_ops,
			  compfull, &maxkey, &place2->key);
}
#endif
