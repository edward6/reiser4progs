/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "obj40.h"

/* The plugin tryes to realize the object: detects the SD, body items. */
errno_t obj40_realize(object_info_t *info, 
		      obj40_realize_func_t mode_func, 
		      obj40_realize_func_t type_func) 
{
	sdext_lw_hint_t lw_hint;
	key_entity_t key;
	lookup_t lookup;
	key_type_t type;
	place_t place;
	errno_t res;

	aal_assert("vpf-1121", info != NULL);
	
	if (info->okey.plugin) {
		/* If the start key is specified it must be the key of StatData. 
		   If the item pointed by this key was found it must SD, check 
		   its mode with mode_func. If item was not found, tryes to detect
		   some body items. */
		
		uint64_t locality, objectid;
		
		/* If offset != 0 - it cannot be a reg40 object start key. */
		if (plugin_call(info->okey.plugin->o.key_ops, get_offset, 
				&info->okey))
			return -EINVAL;
		
		/* If type != SD type - it cannot be a reg40 object start key. */
		if (plugin_call(info->okey.plugin->o.key_ops, get_type, 
				&info->okey) != KEY_STATDATA_TYPE)
			return -EINVAL;

		/* If item was realized - the pointed item was found. */
		if (info->start.item.plugin) {
			if (info->start.item.plugin->h.group != STATDATA_ITEM)
				return -EINVAL;

			/* This is a SD item. It must be a reg SD. */
			if ((res = obj40_read_lw(&info->start.item, &lw_hint)))
				return res;
			
			return mode_func(lw_hint.mode) ? 0 : -EINVAL;
		}
		
		locality = plugin_call(info->okey.plugin->o.key_ops,
				       get_locality, &info->okey);
		objectid = plugin_call(info->okey.plugin->o.key_ops,
				       get_objectid, &info->okey);
		
		/* Item was not realized - the pointed item was not found. 
		   try to find other reg40 items. */
		plugin_call(info->okey.plugin->o.key_ops, build_generic, &key,
			    type, locality, objectid, 0);
		
		lookup = core->tree_ops.lookup(info->tree, &key, 
					       LEAF_LEVEL, &place);
		
		/* If FILEBODY item was found, then it is reg40 body item. */
		if (lookup == PRESENT)
			return 0;
		
		/* If place is invalid, then no one reg40 body item was found. */
		if (!core->tree_ops.valid(info->tree, &place))
			return -EINVAL;
		
		/* Initializing item entity at @next place */
		if ((res = core->tree_ops.realize(info->tree, &place)))
			return res;

		return plugin_call(info->okey.plugin->o.key_ops, compare_short, 
				&info->okey, &key) ? -EINVAL : 0;
	} else {
		/* Realizing by place, If it is a SD - check its mode with mode_func,
		   othewise check the type of the specified item. */
		
		aal_assert("vpf-1122", info->start.item.plugin != NULL);
		
		if (info->start.item.plugin->h.group == STATDATA_ITEM) {
			/* This is a SD item. It must be a reg SD. */
			if ((res = obj40_read_lw(&info->start.item, &lw_hint)))
				return res;
			
			return mode_func(lw_hint.mode) ? 0 : -EINVAL;
		}
		
		type = plugin_call(info->okey.plugin->o.key_ops, get_type, 
				   &info->start.item.key);
		
		return type_func(type);
	}

	return 0;
}

#endif

