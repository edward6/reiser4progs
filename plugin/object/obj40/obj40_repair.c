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
errno_t obj40_stat(obj40_t *obj, stat_func_t stat_func) {
	object_info_t *info;
	errno_t res;

	aal_assert("vpf-1200", obj != NULL);
	
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
	return obj40_stat(obj, stat_func);
}

errno_t obj40_check_stat(obj40_t *obj, nlink_func_t nlink_func, 
			 mode_func_t mode_func, size_func_t size_func,
			 uint64_t size, uint64_t bytes, uint8_t mode)
{
	sdext_lw_hint_t lw_hint, lw_new;
	sdext_unix_hint_t unix_hint;
	errno_t res = RE_OK;
	place_t *stat;
	
	aal_assert("vpf-1213", obj != NULL);
	
	stat = &obj->info.start;
	
	/* Update the SD place. */
	if ((res = obj40_update(obj, stat)))
		return res;
	
	/* Read LW extention. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	/* Form the correct LW extention. */
	lw_new = lw_hint;
	nlink_func(&lw_new.nlink);
	mode_func(&lw_new.mode);
	
	/* Check the mode in the LW extention. */
	if (lw_new.mode != lw_hint.mode) {
		aal_exception_error("Node (%llu), item (%u): StatData of "
				    "the file [%s] has the wrong mode (%u),"
				    "%s (%u). Plugin (%s).", 
				    stat->block->nr, stat->pos.item, 
				    print_ino(obj->core, &stat->key),
				    lw_hint.mode, mode == RM_CHECK ? 
				    "Should be" : "Fixed to", lw_new.mode, 
				    stat->plug->label);
		
		if (mode == RM_CHECK)
			res = RE_FIXABLE;
	}
	
	size_func(&lw_new.size, size);
	
	/* Check the size in the LW extention. */
	if (lw_new.size != lw_hint.size) {
		/* FIXME-VITALY: This is not correct for extents as the last 
		   block can be not used completely. Where to take the policy
		   plugin to figure out if size is correct? */
		aal_exception_error("Node (%llu), item (%u): StatData of "
				    "the file [%s] has the wrong size "
				    "(%llu), %s (%llu). Plugin (%s).",
				    stat->block->nr, stat->pos.item, 
				    print_ino(obj->core, &stat->key),
				    lw_hint.size, mode == RM_CHECK ? 
				    "Should be" : "Fixed to", lw_new.size, 
				    stat->plug->label);
		
		if (mode == RM_CHECK)
			res = RE_FIXABLE;
	}
	
	if ((res |= obj40_read_ext(stat, SDEXT_UNIX_ID, &unix_hint)) < 0)
		return res;
	
	/* Check the mode in the LW extention. */
	if (unix_hint.bytes != bytes) {
		aal_exception_error("Node (%llu), item (%u): StatData of "
				    "the file [%s] has the wrong bytes "
				    "(%llu), %s (%llu). Plugin (%s).", 
				    stat->block->nr, stat->pos.item, 
				    print_ino(obj->core, &stat->key),
				    unix_hint.bytes, mode == RM_CHECK ? 
				    "Should be" : "Fixed to", bytes, 
				    stat->plug->label);
		
		if (mode == RM_CHECK) {
			unix_hint.bytes = bytes;
			res = RE_FIXABLE;
		}
	}
	
	/* Fix r_dev field silently. */
	if (unix_hint.rdev)
		unix_hint.rdev = 0;

	if (mode == RM_CHECK)
		return res;
	
	if ((res = obj40_write_ext(stat, SDEXT_LW_ID, &unix_hint)) < 0)
		return res;
	
	if ((res |= obj40_write_ext(stat, SDEXT_LW_ID, &lw_new)) >= 0) {
		/* FIXME-VITALY: Mark the node dirty. */
	}
		
	
	return res;
}

/* Fix @place->key if differs from @key. */
errno_t obj40_ukey(obj40_t *obj, place_t *place, 
		   key_entity_t *key, uint8_t mode) 
{
	errno_t res;
	
	aal_assert("vpf-1218", obj != NULL);
	
	if (!key->plug->o.key_ops->compfull(key, &place->key))
		return 0;
	
	aal_exception_error("Node (%llu), item(%u): the key [%s] of the "
			    "item is wrong, %s [%s]. Plugin (%s).", 
			    place->block->nr, place->pos.unit, 
			    print_key(obj->core, &place->key),
			    mode == RM_BUILD ? "fixed to" : "should be", 
			    print_key(obj->core, key), obj->plug->label);
	
	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((res = obj->core->tree_ops.ukey(obj->info.tree, place, key))) {
		aal_exception_error("Node (%llu), item(%u): update of the "
				    "item key failed.", place->block->nr,
				    place->pos.unit);
	}

	return res;
}

errno_t obj40_stat_launch(obj40_t *obj, stat_func_t stat_func, 
			  uint32_t nlink, uint16_t fmode, 
			  uint8_t mode)
{
	key_entity_t *key;
	lookup_t lookup;
	place_t *start;
	uint64_t pid;
	errno_t res;

	aal_assert("vpf-1225", obj != NULL);
	
	start = STAT_PLACE(obj);
	key = &obj->info.object;

	/* Update the place of SD. */
	lookup = obj->core->tree_ops.lookup(obj->info.tree, key, 
					    LEAF_LEVEL, start);

	if (lookup == FAILED)
		return -EINVAL;

	if (lookup == PRESENT) {
		if ((res = stat_func(start))) {
			aal_exception_error("Node (%llu), item (%u): StatData "
					    "is not of the current object. "
					    "Plugin (%s)", start->block->nr,
					    start->pos.item, start->plug->label);
		}

		return res;
	}

	/* Absent. If SD is not correct. Create a new one. */
	if ((res = obj40_stat(obj, stat_func)) <= 0)
		return res;
	
	/* Check showed that this is not right SD, create a new one. This is 
	   the special case and usually is not used as object plugin cannot 
	   be realized w/out SD. Used for for "/" and "lost+found" recovery. */
	aal_exception_error("The file [%s] does not have a StatData item. %s"
			    "Plugin %s.", print_ino(obj->core, key), 
			    mode == RM_BUILD ? " Creating a new one." :
			    "",  obj->plug->label);

	if (mode != RM_BUILD)
		return RE_FATAL;
	
	pid = obj->core->profile_ops.value("statdata");
	
	if (pid == INVAL_PID)
		return -EINVAL;
	
	if ((res = obj40_create_stat(obj, pid,  0, 0, nlink, fmode))) {
		aal_exception_error("The file [%s] failed to create a "
				    "StatData item. Plugin %s.", 
				    print_ino(obj->core, key),
				    obj->plug->label);
	}

	return res;
}

#endif
