/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "obj40.h"
#include <repair/plugin.h>

/*
errno_t obj40_check_sd(place_t *sd, realize_func_t func) {
	sdext_lw_hint_t lw_hint;
	statdata_hint_t stat;
	create_hint_t hint;
	
	aal_memset(&stat, 0, sizeof(stat));
	hint.type_specific = &stat;
	
	// Read set of extentions. 
	if (plug_call(sd->plug->o.item_ops, read, sd, &hint, 0, 1) != 1)
		return -EINVAL;
	
	// Check that only LW and UNIX exists. 
	if (stat.extmask != (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID))
		return RE_FATAL;
	
	aal_memset(&lw_hint, 0, sizeof(lw_hint));
	stat.ext[SDEXT_LW_ID] = &lw_hint;
	
	// Read LW extention. 
	if (plug_call(sd->plug->o.item_ops, read, sd, &hint, 0, 1) != 1)
		return -EINVAL;
	
	return func(lw_hint.mode);
}
*/

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
	} else if (info->start.plug->id.group != STATDATA_ITEM) {
		/* Realizing on the place: if not SD, build SD key and try to 
		   find it. Do not recover obj40 without SD at all. */
		
		lookup_t lookup;
		key_type_t type;
		
		type = plug_call(info->start.key.plug->o.key_ops, get_type, 
				 &info->start.key);

		/* Wrong item type. */
		if ((type & types) == 0) 
			return RE_FATAL;

		if ((res = key_func(obj)))
			return -EINVAL;

		lookup = core->tree_ops.lookup(info->tree, &info->object,
					       LEAF_LEVEL, &info->start);

		if (lookup == FAILED)
			return -EINVAL;
	}
	
	/* Check if place is on a SD item probably with Some broken key 
	   (offset != 0). If will be fixed at check_struct time. If not,
	   do not recover objects without SD. */
	if (!info->start.plug) {
		if (!core->tree_ops.valid(info->tree, &info->start))
			return RE_FATAL;

		if ((res = core->tree_ops.fetch(info->tree, &info->start)))
			return -EINVAL;

		if (info->start.plug->id.group != STATDATA_ITEM)
			return RE_FATAL;
	}

	return sd_func(&info->start);
}

#endif

