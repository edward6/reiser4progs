/*
  common40.c -- item plugins common code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/plugin.h>
#include "common40.h"

/*
  Builds the key of the unit at @pos and stores it inside passed @key
  variable. It is needed for updating item key after shifting, etc.
*/
errno_t common40_get_key(item_entity_t *item,
			 uint64_t pos,
			 key_entity_t *key,
			 trans_func_t trans_func)
{
#ifndef ENABLE_STAND_ALONE
	uint64_t offset;
#else
	uint32_t offset;
#endif

	plugin_call(item->key.plugin->key_ops, assign,
		    key, &item->key);
		
	offset = plugin_call(key->plugin->key_ops,
			     get_offset, key);

	offset += (trans_func ? trans_func(item, pos) : pos);

	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset);
	
	return 0;
}

/* Returns maximal possible key from file body items */
errno_t common40_maxposs_key(item_entity_t *item,
			     key_entity_t *key) 
{
#ifndef ENABLE_STAND_ALONE
	uint64_t offset;
#else
	uint32_t offset;
#endif
	key_entity_t *maxkey;
    
	plugin_call(item->key.plugin->key_ops, assign,
		    key, &item->key);
    
	maxkey = plugin_call(key->plugin->key_ops,
			     maximal,);
    
	offset = plugin_call(key->plugin->key_ops,
			     get_offset, maxkey);
	
    	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE

/* Returns max real key inside passed @item */
errno_t common40_maxreal_key(item_entity_t *item,
			     key_entity_t *key,
			     trans_func_t trans_func) 
{
#ifndef ENABLE_STAND_ALONE
	uint64_t units;
	uint64_t offset;
#else
	uint32_t units;
	uint32_t offset;
#endif

	units = plugin_call(item->plugin->item_ops,
			    units, item);
	
	plugin_call(item->key.plugin->key_ops,
		    assign, key, &item->key);

	offset = plugin_call(key->plugin->key_ops,
			     get_offset, key);

	if (trans_func)
		units = trans_func(item, units);
	
	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset + units - 1);
	
	return 0;
}

/* Checks if two items are mergeable */
int common40_mergeable(item_entity_t *item1,
		       item_entity_t *item2)
{
#ifndef ENABLE_STAND_ALONE
	uint64_t maxreal, offset;
#else
	uint32_t maxreal, offset;
#endif
	key_entity_t maxreal_key;
	reiser4_plugin_t *plugin;
	oid_t objectid1, objectid2;
	
	plugin = item1->key.plugin;
	
	objectid1 = plugin_call(plugin->key_ops, get_locality,
				&item1->key);
	
	objectid2 = plugin_call(plugin->key_ops, get_locality,
				&item2->key);

	if (objectid1 != objectid2)
		return 0;
	
	objectid1 = plugin_call(plugin->key_ops, get_objectid,
				&item1->key);
	
	objectid2 = plugin_call(plugin->key_ops, get_objectid,
				&item2->key);

	if (objectid1 != objectid2)
		return 0;

	plugin_call(item1->plugin->item_ops, maxreal_key,
		    item1, &maxreal_key);
		
	maxreal = plugin_call(plugin->key_ops, get_offset,
			      &maxreal_key);
	
	offset = plugin_call(plugin->key_ops, get_offset,
			     &item2->key);

	if (maxreal != offset)
		return 0;
	
	return 1;
}
#endif

lookup_t common40_lookup(item_entity_t *item,
			 key_entity_t *key,
			 uint64_t *pos,
			 trans_func_t trans_func)
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

	common40_maxposs_key(item, &maxkey);

	units = plugin_call(item->plugin->item_ops,
			    units, item);
	if (units == 0)
		return LP_ABSENT;

	size = trans_func ? trans_func(item, units) :
		units;
	
	if (plugin_call(key->plugin->key_ops,
			compare, key, &maxkey) > 0)
	{
		*pos = size;
		return LP_ABSENT;
	}

	offset = plugin_call(key->plugin->key_ops,
			     get_offset, &item->key);

	wanted = plugin_call(key->plugin->key_ops,
			     get_offset, key);

	if (wanted >= offset &&
	    wanted < offset + size)
	{
		*pos = wanted - offset;
		return LP_PRESENT;
	}

	*pos = size;
	return LP_ABSENT;
}
