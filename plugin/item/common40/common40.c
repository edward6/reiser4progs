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
			 uint32_t pos,
			 key_entity_t *key,
			 trans_func_t func)
{
	uint64_t offset;

	plugin_call(item->key.plugin->key_ops, assign,
		    key, &item->key);
		
	offset = plugin_call(key->plugin->key_ops,
			     get_offset, key);

	offset += (func ? func(item, pos) : pos);

	plugin_call(key->plugin->key_ops, set_offset,
		    key, offset);
	
	return 0;
}

/* Returns maximal possible key from file body items */
errno_t common40_maxposs_key(item_entity_t *item,
			     key_entity_t *key) 
{
	uint64_t offset;
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

/* Checks if two items are mergeable */
int common40_mergeable(item_entity_t *item1,
		       item_entity_t *item2)
{
	key_entity_t utmost_key;
	uint64_t utmost, offset;
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

	plugin_call(item1->plugin->item_ops, utmost_key,
		    item1, &utmost_key);
		
	utmost = plugin_call(plugin->key_ops, get_offset,
			     &utmost_key);
	
	offset = plugin_call(plugin->key_ops, get_offset,
			     &item2->key);

	if (utmost != offset)
		return 0;
	
	return 1;
}
