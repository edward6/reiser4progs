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
errno_t obj40_realize(object_info_t *info,
		      realize_sd_func_t sd_func,
		      realize_body_func_t body_func,
		      uint64_t item_types)
{
	sdext_lw_hint_t lw_hint;
	key_entity_t key;
	key_type_t type;
	uint64_t mask;
	errno_t res;

	aal_assert("vpf-1121", info != NULL);
	aal_assert("vpf-1121", info->tree != NULL);
	aal_assert("vpf-1127", info->object.plug || info->start.plug);
	
	aal_memset(&lw_hint, 0, sizeof(lw_hint));
	
	if (info->object.plug) {
		/* If the start key is specified it must be the key of
		   StatData. If the item pointed by this key was found it must
		   SD, check its mode with mode_func. If item was not found,
		   tries to detect some body items. */
		
		uint64_t locality, objectid, ordering;
		lookup_t lookup;
		place_t place;
	
		locality = plug_call(info->object.plug->o.key_ops,
				     get_locality, &info->object);
		
		objectid = plug_call(info->object.plug->o.key_ops,
				     get_objectid, &info->object);
		
		ordering = plug_call(info->object.plug->o.key_ops,
				     get_ordering, &info->object);

		plug_call(info->object.plug->o.key_ops, build_gener, &key,
			  KEY_STATDATA_TYPE, locality, ordering, objectid, 0);
		
		/* Object key must be a key of SD. */
		if (plug_call(info->object.plug->o.key_ops, compfull, &key, 
			      &info->object))
			return RE_FATAL;
		
		/* If StatData is realized - check taht it is reg40 SD. */
		if (info->start.plug)
			return sd_func(&info->start);
		
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
		
		/* If place is invalid, then no one reg40 body item is found. */
		if (!core->tree_ops.valid(info->tree, &place))
			return RE_FATAL;
		
		/* Initializing item entity at @next place */
		if ((res = core->tree_ops.fetch(info->tree, &place)))
			return res;
		
		return plug_call(info->object.plug->o.key_ops, compshort,
				 &info->object, &key) ? RE_FATAL : 0;
	}

	/* Realizing by place */
	aal_assert("vpf-1122", info->start.plug != NULL);

	if (info->start.plug->id.group == STATDATA_ITEM)
		return sd_func(&info->start);

	type = plug_call(info->start.key.plug->o.key_ops, get_type, 
			 &info->start.key);

	return type & item_types ? 0 : RE_FATAL;
}

#endif

