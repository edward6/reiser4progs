/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   body40.c -- file body item plugins common code. */

#include <reiser4/plugin.h>
#include "body40.h"

/* Builds the key of the unit at @pos and stores it inside passed @key
   variable. It is needed for updating item key after shifting, etc. */
errno_t body40_get_key(item_entity_t *item,
		       uint32_t pos,
		       key_entity_t *key,
		       trans_func_t trans_func)
{
#ifndef ENABLE_STAND_ALONE
	uint64_t offset;
#else
	uint32_t offset;
#endif

	plugin_call(item->key.plugin->o.key_ops, assign,
		    key, &item->key);
		
	offset = plugin_call(key->plugin->o.key_ops,
			     get_offset, key);

	offset += (trans_func ? trans_func(item, pos) : pos);

	plugin_call(key->plugin->o.key_ops, set_offset,
		    key, offset);
	
	return 0;
}

/* Returns maximal possible key from file body items */
errno_t body40_maxposs_key(item_entity_t *item,
			   key_entity_t *key) 
{
#ifndef ENABLE_STAND_ALONE
	uint64_t offset;
#else
	uint32_t offset;
#endif
	
	key_entity_t *maxkey;
    
	plugin_call(item->key.plugin->o.key_ops, assign,
		    key, &item->key);
    
	maxkey = plugin_call(key->plugin->o.key_ops,
			     maximal);
    
	offset = plugin_call(key->plugin->o.key_ops,
			     get_offset, maxkey);
	
    	plugin_call(key->plugin->o.key_ops, set_offset,
		    key, offset);

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Returns max real key inside passed @item */
errno_t body40_maxreal_key(item_entity_t *item,
			   key_entity_t *key,
			   trans_func_t trans_func) 
{
	uint64_t units;
	uint64_t offset;

	units = plugin_call(item->plugin->o.item_ops,
			    units, item);
	
	plugin_call(item->key.plugin->o.key_ops,
		    assign, key, &item->key);

	offset = plugin_call(key->plugin->o.key_ops,
			     get_offset, key);

	offset += (trans_func ? trans_func(item, units) :
		   units);
	
	plugin_call(key->plugin->o.key_ops, set_offset,
		    key, offset - 1);
	
	return 0;
}

/* Checks if two items are mergeable */
int body40_mergeable(item_entity_t *item1,
		     item_entity_t *item2)
{
	uint64_t offset;
	key_entity_t maxreal_key;

	plugin_call(item1->plugin->o.item_ops, maxreal_key,
		    item1, &maxreal_key);

	offset = plugin_call(maxreal_key.plugin->o.key_ops,
			     get_offset, &maxreal_key);

	plugin_call(maxreal_key.plugin->o.key_ops,
		    set_offset, &maxreal_key, offset + 1);
	
	return !plugin_call(item1->key.plugin->o.key_ops,
			    compfull, &maxreal_key, &item2->key);
}
#endif

lookup_t body40_lookup(item_entity_t *item,
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

	body40_maxposs_key(item, &maxkey);

	if (!(units = plugin_call(item->plugin->o.item_ops,
				  units, item)))
	{
		return ABSENT;
	}

	size = trans_func ? trans_func(item, units) : units;
	
	if (plugin_call(key->plugin->o.key_ops,
			compfull, key, &maxkey) > 0)
	{
		*pos = size;
		return ABSENT;
	}

	offset = plugin_call(key->plugin->o.key_ops,
			     get_offset, &item->key);

	wanted = plugin_call(key->plugin->o.key_ops,
			     get_offset, key);

	if (wanted >= offset &&
	    wanted < offset + size)
	{
		*pos = wanted - offset;
		return PRESENT;
	}

	*pos = size;
	return ABSENT;
}
