/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */

#ifndef ENABLE_STAND_ALONE
#include "obj40.h"
#include <repair/plugin.h>

/* Obtains the plugin of the type @type from SD if stored there, otherwise
   obtains the default one from the params. This differs from obj40_plug as it
   checks if the id from the SD is a valid one. */
reiser4_plug_t *obj40_plug_recognize(obj40_t *obj, rid_t type, char *name) {
	reiser4_plug_t *plug;
	rid_t pid;
	
	aal_assert("vpf-1237", obj != NULL);
	aal_assert("vpf-1238", STAT_PLACE(obj)->plug != NULL);
	
	pid = plug_call(STAT_PLACE(obj)->plug->o.item_ops->object,
			object_plug, STAT_PLACE(obj), type);
	
	/* If id found, try to find the plugin. */
	if (pid != INVAL_PID) {
		if ((plug = obj->core->factory_ops.ifind(type, pid)) != NULL)
			return plug;

		/* FIXME-VITALY: This is probably wrong -- if there is a hash
		   plugin in SD saved, that means that it is not standard and
		   it is probably should be detected. For now the default one 
		   is used. Hash detection it to be done sometimes later.  */
	}
	
	/* Id either is not kept in SD or has not been found, 
	   obtain the default one. */
	if ((pid = obj->core->param_ops.value(name)) == INVAL_PID)
		return NULL;
	
	return obj->core->factory_ops.ifind(type, pid);
}

/* Checks that @obj->info.start is SD of the wanted file.  */
errno_t obj40_stat(obj40_t *obj, stat_func_t stat_func) {
	object_info_t *info;
	errno_t res;

	aal_assert("vpf-1200", obj != NULL);
	
	info = &obj->info;
	
	if (!info->start.plug) {
		if (!obj->core->tree_ops.valid(info->tree, &info->start))
			return RE_FATAL;

		if ((res = obj->core->tree_ops.fetch(info->tree, &info->start)))
			return -EINVAL;
	}
	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return RE_FATAL;
	
	/* Is @info->start SD of the wanted file? If some fields are broken, 
	   like offset != 0, fix it at check_struct time. */
	if (info->object.plug->o.key_ops->compshort(&info->object, 
						    &info->start.key))
	{
		return RE_FATAL;
	}
	
	/* Some SD is recognized. Check that this is our SD. */
	return stat_func ? stat_func(&info->start) : 0;
}

/* The plugin tries to recognize the object: detects the SD, body items */
errno_t obj40_recognize(obj40_t *obj, stat_func_t stat_func) {
	uint64_t locality, objectid, ordering;
	object_info_t *info;
	reiser4_key_t key;

	aal_assert("vpf-1121", obj->info.tree != NULL);
	aal_assert("vpf-1127", obj->info.object.plug || obj->info.start.plug);
	
	info = &obj->info;
	
	if (info->object.plug) {
		locality = plug_call(info->object.plug->o.key_ops,
				     get_locality, &info->object);

		objectid = plug_call(info->object.plug->o.key_ops,
				     get_objectid, &info->object);

		ordering = plug_call(info->object.plug->o.key_ops,
				     get_ordering, &info->object);

	
		plug_call(info->object.plug->o.key_ops, build_generic, &key,
			  KEY_STATDATA_TYPE, locality, ordering, objectid, 0);
		
		/* Realizing on the key: SD is not found. Check if the key 
		   pointer is correct. */
		if (plug_call(info->object.plug->o.key_ops, compfull, 
			      &key, &info->object))
			return RE_FATAL;
	} else {
		/* Realizing on the SD. */
		aal_assert("vpf-1204",  info->start.plug->id.group == 
			   		STATDATA_ITEM);

		locality = plug_call(info->object.plug->o.key_ops,
				     get_locality, &info->start.key);

		objectid = plug_call(info->object.plug->o.key_ops,
				     get_objectid, &info->start.key);

		ordering = plug_call(info->object.plug->o.key_ops,
				     get_ordering, &info->start.key);

		/* Build the SD key into @info->object. */
		plug_call(info->start.key.plug->o.key_ops, build_generic, 
			  &info->object, KEY_STATDATA_TYPE, locality, 
			  ordering, objectid, 0);
	}
	
	/* @info->object is the key of SD for now and @info->start is the 
	   result of tree lookup by @info->object -- skip objects w/out SD. */
	return obj40_stat(obj, stat_func);
}

errno_t obj40_check_stat(obj40_t *obj, nlink_func_t nlink_func, 
			 mode_func_t mode_func, size_func_t size_func,
			 uint64_t size, uint64_t bytes, uint8_t mode)
{
	sdext_lw_hint_t lw_hint, lw_new;
	sdext_unix_hint_t unix_hint;
	errno_t res = 0;
	reiser4_place_t *stat;
	
	aal_assert("vpf-1213", obj != NULL);
	
	stat = &obj->info.start;
	
	/* Update the SD place. */
	if ((res = obj40_update(obj))) {
		aal_error("Node (%llu), item (%u): failed to update "
			  "the StatData of the file [%s]. Plugin (%s).", 
			  stat->node->block->nr, stat->pos.item,
			  print_inode(obj->core, &stat->key), 
			  stat->plug->label);
		return res;
	}
	
	/* Read LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)))
		return res;
	
	/* Form the correct LW extension. */
	lw_new = lw_hint;
	
	if (nlink_func) {
		nlink_func(obj, &lw_new.nlink);
		if (lw_new.nlink != lw_hint.nlink)
			res = RE_FIXABLE;
	}

	mode_func(obj, &lw_new.mode);
	
	/* Check the mode in the LW extension. */
	if (lw_new.mode != lw_hint.mode) {
		aal_error("Node (%llu), item (%u): StatData of the file [%s] "
			  "has the wrong mode (%u), %s (%u). Plugin (%s).",
			  stat->node->block->nr, stat->pos.item, 
			  print_inode(obj->core, &stat->key),
			  lw_hint.mode, mode == RM_CHECK ? "Should be" : 
			  "Fixed to", lw_new.mode, stat->plug->label);
		
		res = RE_FIXABLE;
	}
	
	size_func(obj, &lw_new.size, size);
	
	/* Check the size in the LW extension. */
	if (lw_new.size != lw_hint.size) {
		aal_error("Node (%llu), item (%u): StatData of the file [%s] "
			  "has the wrong size (%llu), %s (%llu). Plugin (%s).",
			  stat->node->block->nr, stat->pos.item, 
			  print_inode(obj->core, &stat->key),
			  lw_hint.size, mode == RM_CHECK ? "Should be" : 
			  "Fixed to", lw_new.size, stat->plug->label);
		
		res = RE_FIXABLE;
	}
	
	if (res && mode != RM_CHECK)  {
		res = obj40_write_ext(stat, SDEXT_LW_ID, &lw_new);
		if (res) return res;
	}
	
	if ((res |= obj40_read_ext(stat, SDEXT_UNIX_ID, &unix_hint)) < 0)
		return res;
	
	/* Check the mode in the LW extension. */
	
	/* sd_butes are set wrongly in the kernel. Waiting for the VS. */
	if (bytes != MAX_UINT64 && unix_hint.bytes != bytes) {
		aal_error("Node (%llu), item (%u): StatData of the file [%s] "
			  "has the wrong bytes (%llu), %s (%llu). Plugin (%s).",
			  stat->node->block->nr, stat->pos.item, 
			  print_inode(obj->core, &stat->key),
			  unix_hint.bytes, mode == RM_CHECK ? "Should be" : 
			  "Fixed to", bytes, stat->plug->label);
		
		unix_hint.bytes = bytes;
		unix_hint.rdev = 0;
		res = RE_FIXABLE;
	}
	
	if (res && mode != RM_CHECK) {
		res = obj40_write_ext(stat, SDEXT_UNIX_ID, &unix_hint);
		if (res) return res;
	}
	
	return res;
}

/* Fix @place->key if differs from @key. */
errno_t obj40_fix_key(obj40_t *obj, reiser4_place_t *place, 
		      reiser4_key_t *key, uint8_t mode) 
{
	errno_t res;
	
	aal_assert("vpf-1218", obj != NULL);
	
	if (!key->plug->o.key_ops->compfull(key, &place->key))
		return 0;
	
	aal_error("Node (%llu), item(%u): the key [%s] of the item is "
		  "wrong, %s [%s]. Plugin (%s).", place->node->block->nr,
		  place->pos.unit, print_key(obj->core, &place->key),
		  mode == RM_BUILD ? "fixed to" : "should be", 
		  print_key(obj->core, key), obj->plug->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	if ((res = obj->core->tree_ops.update_key(obj->info.tree,
						  place, key)))
	{
		aal_error("Node (%llu), item(%u): update of the "
			  "item key failed.", place->node->block->nr,
			  place->pos.unit);
	}

	return res;
}

errno_t obj40_launch_stat(obj40_t *obj, stat_func_t stat_func, 
			  uint64_t mask, uint32_t nlink, 
			  uint16_t objmode, uint8_t mode)
{
	reiser4_key_t *key;
	lookup_t lookup;
	errno_t res = 0;
	reiser4_place_t *start;
	uint64_t pid;

	aal_assert("vpf-1225", obj != NULL);
	
	start = STAT_PLACE(obj);
	key = &obj->info.object;

	/* Update the place of SD. */
	if ((lookup = obj40_lookup(obj, key, LEAF_LEVEL, FIND_EXACT, 
				   NULL, NULL, start)) < 0)
		return lookup;

	if (lookup == PRESENT) {
		/* FIXME-VITALY: fix the found SD if needed. */
		if (stat_func && (res = stat_func(start))) {
			aal_error("Node (%llu), item (%u): StatData "
				  "is not of the current object. "
				  "Plugin (%s)", start->node->block->nr,
				  start->pos.item, start->plug->label);
		}

		return res;
	}

	/* Absent. If SD is not correct. Create a new one. */
	if ((res = obj40_stat(obj, stat_func)) <= 0)
		return res;
	
	/* Check showed that this is not right SD, create a new one. 
	   
	   THIS IS THE SPECIAL CASE and usually is not used as object plugin 
	   cannot be recognized w/out SD. Used for for "/" and "lost+found" 
	   recovery. */
	
	aal_error("The file [%s] does not have a StatData item.%s"
		  " Plugin %s.", print_inode(obj->core, key), 
		  mode == RM_BUILD ? " Creating a new one." :
		  "",  obj->plug->label);

	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((pid = obj->core->param_ops.value("statdata") == INVAL_PID))
		return -EINVAL;

	if ((res = obj40_create_stat(obj, pid, mask, 0, 0,
				     0, nlink, objmode, NULL)))
	{
		aal_error("The file [%s] failed to create a "
			  "StatData item. Plugin %s.", 
			  print_inode(obj->core, key),
			  obj->plug->label);
	}

	return res;
}
#endif
