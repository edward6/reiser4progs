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
errno_t obj40_realize(obj40_t *obj, realize_func_t sd_func, 
		      realize_key_func_t key_func, uint64_t types)
{
	object_info_t *info;
	errno_t res;

	aal_assert("vpf-1121", info != NULL);
	aal_assert("vpf-1121", obj->info.tree != NULL);
	aal_assert("vpf-1127", obj->info.object.plug || obj->info.start.plug);
	
	info = &obj->info;
	
	if (info->object.plug) {
		/* Realizing on the key: SD is not found. Check if the key 
		   pointer is correct. */
		
		uint64_t locality, objectid, ordering;
		key_entity_t key;
		
		locality = plug_call(info->object.plug->o.key_ops,
				     get_locality, &info->object);
		
		objectid = plug_call(info->object.plug->o.key_ops,
				     get_objectid, &info->object);
		
		ordering = plug_call(info->object.plug->o.key_ops,
				     get_ordering, &info->object);

		plug_call(info->object.plug->o.key_ops, build_gener, &key,
			  KEY_STATDATA_TYPE, locality, ordering, objectid, 0);
		
		if (plug_call(info->object.plug->o.key_ops, compfull, &key, 
			      &info->object))
			return RE_FATAL;
	} else {
		/* Realizing on the place: if not SD, build SD key and 
		   try to find it. Do not recover obj40 without SD at all. */
		
		/* Build the SD key into @info->object. */
		if ((res = key_func(obj)))
			return -EINVAL;
		
		if (info->start.plug->id.group != STATDATA_ITEM) {
			lookup_t lookup;
			key_type_t type;

			type = plug_call(info->start.key.plug->o.key_ops, 
					 get_type, &info->start.key);

			/* Wrong item type. */
			if ((type & types) == 0) 
				return RE_FATAL;

			lookup = core->tree_ops.lookup(info->tree, 
						       &info->object,
						       LEAF_LEVEL, 
						       &info->start);

			if (lookup == FAILED)
				return -EINVAL;
		}
	}
	
	/* @info->object is the key of SD for now and @info->start is the 
	   result of tree lookup by @info->object -- skip objects w/out SD. */
	if (!info->start.plug) {
		if (!core->tree_ops.valid(info->tree, &info->start))
			return RE_FATAL;

		if ((res = core->tree_ops.fetch(info->tree, &info->start)))
			return -EINVAL;
	}
	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return RE_FATAL;
	
	/* Is @info->start SD of the wanted file? If some fields are broken, 
	   like offset != 0, fix it at check_struct time. */
	if (info->object.plug->o.key_ops->compshort(&info->object, 
						    &info->start.key))
		return RE_FATAL;
	
	/* Some SD is realized. Check that this is our SD. */
	return sd_func(&info->start);
}

#endif

