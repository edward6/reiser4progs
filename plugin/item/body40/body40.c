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
#ifndef ENABLE_STAND_ALONE
	uint64_t offset;
#else
	uint32_t offset;
#endif

	plug_call(place->key.plug->o.key_ops, assign,
		  key, &place->key);
		
	offset = plug_call(key->plug->o.key_ops,
			   get_offset, key);

	offset += (trans_func ? trans_func(place, pos) : pos);

	plug_call(key->plug->o.key_ops, set_offset,
		  key, offset);
	
	return 0;
}

/* Returns maximal possible key from file body items */
errno_t body40_maxposs_key(place_t *place, key_entity_t *key) {
#ifndef ENABLE_STAND_ALONE
	uint64_t offset;
#else
	uint32_t offset;
#endif
	
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
/* Returns max real key inside passed @place */
errno_t body40_maxreal_key(place_t *place,
			   key_entity_t *key,
			   trans_func_t trans_func) 
{
	uint64_t units;
	uint64_t offset;

	units = plug_call(place->plug->o.item_ops,
			  units, place);
	
	plug_call(place->key.plug->o.key_ops,
		  assign, key, &place->key);

	offset = plug_call(key->plug->o.key_ops,
			   get_offset, key);

	offset += (trans_func ? trans_func(place, units) :
		   units);
	
	plug_call(key->plug->o.key_ops, set_offset,
		  key, offset - 1);
	
	return 0;
}

/* Checks if two items are mergeable */
int body40_mergeable(place_t *place1, place_t *place2) {
	uint64_t offset;
	key_entity_t maxreal_key;

	plug_call(place1->plug->o.item_ops, maxreal_key,
		  place1, &maxreal_key);

	offset = plug_call(maxreal_key.plug->o.key_ops,
			   get_offset, &maxreal_key);

	plug_call(maxreal_key.plug->o.key_ops,
		  set_offset, &maxreal_key, offset + 1);
	
	return !plug_call(place1->key.plug->o.key_ops,
			  compfull, &maxreal_key, &place2->key);
}
#endif

lookup_res_t body40_lookup(place_t *place, key_entity_t *key,
			   uint64_t *pos, trans_func_t trans_func)
{
#ifndef ENABLE_STAND_ALONE
	uint64_t size;
	uint64_t offset;
	uint64_t wanted;
#else
	uint32_t size;
	uint32_t offset;
	uint32_t wanted;
#endif
	
	uint32_t units;
	key_entity_t maxkey;

	body40_maxposs_key(place, &maxkey);

	if (!(units = plug_call(place->plug->o.item_ops,
				units, place)))
	{
		return ABSENT;
	}

	size = trans_func ? trans_func(place, units) : units;
	
	if (plug_call(key->plug->o.key_ops,
		      compfull, key, &maxkey) > 0)
	{
		*pos = size;
		return ABSENT;
	}

	offset = plug_call(key->plug->o.key_ops,
			   get_offset, &place->key);

	wanted = plug_call(key->plug->o.key_ops,
			   get_offset, key);

	if (wanted >= offset &&
	    wanted < offset + size)
	{
		*pos = wanted - offset;
		return PRESENT;
	}

	*pos = size;
	return PRESENT;
}
