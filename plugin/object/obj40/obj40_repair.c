/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "obj40.h"
#include <repair/plugin.h>

/* The plugin tries to realize the object: detects the SD, body items */
errno_t obj40_realize(object_info_t *info, 
		      realize_func_t mode_func, 
		      realize_func_t type_func,
		      realize_func_body_t body_func) 
{
	sdext_lw_hint_t lw_hint;
	key_entity_t key;
	lookup_t lookup;
	key_type_t type;
	place_t place;
	errno_t res;

	aal_assert("vpf-1121", info != NULL);
	aal_assert("vpf-1121", info->tree != NULL);
	aal_assert("vpf-1127", info->object.plug || info->start.plug);
	
	if (info->object.plug) {
		/* If the start key is specified it must be the key of
		   StatData. If the item pointed by this key was found it must
		   SD, check its mode with mode_func. If item was not found,
		   tryes to detect some body items. */
		
		uint64_t locality, objectid;
		
		locality = plug_call(info->object.plug->o.key_ops,
				     get_locality, &info->object);
		
		objectid = plug_call(info->object.plug->o.key_ops,
				     get_objectid, &info->object);

		/* FIXME-UMKA->VITALY: Here also should be used right 
		   ordering if we're using large keys. */
		plug_call(info->object.plug->o.key_ops, build_gener, &key,
			  KEY_STATDATA_TYPE, locality, 0, objectid, 0);
		
		/* Object key must be a key of SD. */
		if (plug_call(info->object.plug->o.key_ops, compfull, &key, 
			      &info->object))
			return REPAIR_FATAL;
		
		/* If item was realized - the pointed item was found. */
		if (info->start.plug) {
			/* All objects40 has a StatData at the beginning. */
			if (info->start.plug->id.group != STATDATA_ITEM)
				return REPAIR_FATAL;
			
			/* This is a SD item. It must be a reg SD. */
			if ((res = obj40_read_lw(&info->start, &lw_hint)))
				return res;
			
			return mode_func(lw_hint.mode) ? REPAIR_FATAL : 0;
		}
		
		/* Start item pointed by @info->object key cannot be found 
		   -- build the body key and try to find object body. */
		if ((res = body_func(info, &key)))
			return res;
		
		lookup = core->tree_ops.lookup(info->tree, &key, 
					       LEAF_LEVEL, &place);
		
		if (lookup == PRESENT)
			return 0;
		else if (lookup == FAILED)
			return -EINVAL;
		
		/* If place is invalid, then no one reg40 body item was found. */
		if (!core->tree_ops.valid(info->tree, &place))
			return REPAIR_FATAL;
		
		/* Initializing item entity at @next place */
		if ((res = core->tree_ops.fetch(info->tree, &place)))
			return res;
		
		return plug_call(info->object.plug->o.key_ops, compshort,
				 &info->object, &key) ? REPAIR_FATAL : 0;
	} else {
		/* Realizing by place, If it is a SD - check its mode with 
		   mode_func, othewise check the type of the specified item. */
		aal_assert("vpf-1122", info->start.plug != NULL);
		
		if (info->start.plug->id.group == STATDATA_ITEM) {
			/* This is a SD item. It must be a reg SD. */
			if ((res = obj40_read_lw(&info->start, &lw_hint)))
				return res;
			
			return mode_func(lw_hint.mode) ? REPAIR_FATAL : 0;
		}
		
		type = plug_call(info->object.plug->o.key_ops, get_type, 
				 &info->start.key);
		
		return type_func(type) ? REPAIR_FATAL : 0;
	}

	return 0;
}

#endif

