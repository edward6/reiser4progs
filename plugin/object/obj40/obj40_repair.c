/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "obj40.h"
#include <repair/plugin.h>

/* Checks that @obj->info.start is SD of the wanted file.  */
errno_t obj40_check_stat(obj40_t *obj, stat_func_t stat_func) {
	object_info_t *info;
	errno_t res;

	aal_assert("vpf-1200", obj != NULL);
	
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
	return stat_func(&info->start);
}

/* The plugin tries to realize the object: detects the SD, body items */
errno_t obj40_realize(obj40_t *obj, stat_func_t stat_func, 
		      key_func_t key_func)
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
		/* Realizing on the SD. */
		aal_assert("vpf-1204",  info->start.plug->id.group == 
			   		STATDATA_ITEM);
		
		/* Build the SD key into @info->object. */
		if ((res = key_func(obj)))
			return -EINVAL;
	}
	
	/* @info->object is the key of SD for now and @info->start is the 
	   result of tree lookup by @info->object -- skip objects w/out SD. */
	return obj40_check_stat(obj, stat_func);
}

#endif

