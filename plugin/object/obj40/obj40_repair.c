/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */

#ifndef ENABLE_STAND_ALONE

#include "obj40_repair.h"

/* Obtains the plugin of the type @type from SD if stored there, otherwise
   obtains the default one from the params. This differs from obj40_plug as it
   checks if the id from the SD is a valid one. 
   FIXME: similar to obj40_plug. Eliminate it when plugin extention is ready. */
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
		if (!obj40_valid_item(&info->start))
			return RE_FATAL;

		if ((res = obj40_fetch_item(&info->start)))
			return res;
	}
	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return RE_FATAL;
	
	/* Compare the correct key with the place key. */
	if (plug_call(info->object.plug->o.key_ops, compfull,
		      &info->object, &info->start.key))
		return RE_FATAL;
	
	/* Some SD is recognized. Check that this is our SD. */
	return stat_func ? stat_func(&info->start) : 0;
}

/* The plugin tries to recognize the object: detects the SD, body items */
errno_t obj40_recognize(obj40_t *obj, stat_func_t stat_func) {
	uint64_t locality, objectid, ordering;
	object_info_t *info;
	reiser4_key_t key;

	aal_assert("vpf-1121", obj->info.tree != NULL);
	aal_assert("vpf-1127", obj->info.object.plug != NULL);
	
	info = &obj->info;
	
	/* Check if the key pointer is correct and then check the found item 
	   if it is SD with the proper key. */
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);

	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);

	ordering = plug_call(info->object.plug->o.key_ops,
			     get_ordering, &info->object);

	plug_call(info->object.plug->o.key_ops, build_generic, &key,
		  KEY_STATDATA_TYPE, locality, ordering, objectid, 0);

	/* Compare the correct key with the search one. */
	if (plug_call(info->object.plug->o.key_ops, compfull, 
		      &key, &info->object))
		return RE_FATAL;
	
	/* @info->object is the key of SD for now and @info->start is the 
	   result of tree lookup by @info->object -- skip objects w/out SD. */
	return obj40_stat(obj, stat_func);
}

#define OBJ40_CHECK(field, type, value, correct)			\
static inline int obj40_check_##field(obj40_t *obj,			\
				      type *value,			\
				      type correct)			\
{									\
	if (*value == correct)						\
		return 0;						\
									\
	*value = correct;						\
	return 1;							\
}

/* The default method for nlink and size handling. */
OBJ40_CHECK(nlink, uint32_t, value, correct);
OBJ40_CHECK(size,  uint64_t, value, correct);
OBJ40_CHECK(bytes,  uint64_t, value, correct);

static inline int obj40_check_mode(obj40_t *obj, 
				   uint16_t *mode, 
				   uint16_t correct) 
{
	if (((*mode) & S_IFMT) == correct)
		return 0;

	*mode &= ~S_IFMT;
	*mode |= correct;
	return 1;
}

static inline errno_t obj40_check_lw_ext(obj40_t *obj, 
					 obj40_stat_methods_t *methods,
					 obj40_stat_params_t *params, 
					 uint8_t mode)
{
	reiser4_place_t *stat = &obj->info.start;
	sdext_lw_hint_t hint, correct;
	errno_t res;
	int fixed;

	aal_memset(&hint, 0, sizeof(hint));
	
	/* Read LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &hint)))
		return res;

	/* Form the correct LW extension. */
	correct = hint;

	if (methods->check_nlink == NULL) {
		/* Call the default one. */
		fixed = obj40_check_nlink(obj, &correct.nlink, params->nlink);
	} else if (methods->check_nlink != SKIP_METHOD) {
		fixed = methods->check_nlink(obj, &correct.nlink, params->nlink);
	} else {
		fixed = 0;
	}

	if (fixed) {
		/* Fix nlink silently: there is no way to check if nlink is 
		   correct, so the check is either skipped or rebuild. */
		res = RE_FIXABLE;
	}

	if (methods->check_mode == NULL) {
		/* Call the default one. */
		fixed = obj40_check_mode(obj, &correct.mode, params->mode);
	} else if (methods->check_mode != SKIP_METHOD) {
		fixed = methods->check_mode(obj, &correct.mode, params->mode);
	} else {
		fixed = 0;
	}

	if (fixed) {
		aal_error("Node (%llu), item (%u): StatData of the file [%s] "
			  "has the wrong mode (%u), %s (%u). Plugin (%s).",
			  stat->node->block->nr, stat->pos.item, 
			  print_inode(obj->core, &stat->key),
			  hint.mode, mode == RM_CHECK ? "Should be" : 
			  "Fixed to", correct.mode, stat->plug->label);

		res = RE_FIXABLE;
	}

	if (methods->check_size == NULL) {
		/* Call the default one. */
		fixed = obj40_check_size(obj, &correct.size, params->size);
	} else if (methods->check_size != SKIP_METHOD) {
		fixed = methods->check_size(obj, &correct.size, params->size);
	} else {
		fixed = 0;
	}

	if (fixed) {
		aal_error("Node (%llu), item (%u): StatData of the file [%s] "
			  "has the wrong size (%llu), %s (%llu). Plugin (%s).",
			  stat->node->block->nr, stat->pos.item, 
			  print_inode(obj->core, &stat->key),
			  hint.size, mode == RM_CHECK ? "Should be" : 
			  "Fixed to", correct.size, stat->plug->label);

		res = RE_FIXABLE;
	}

	if (res && mode != RM_CHECK)
		return obj40_write_ext(stat, SDEXT_LW_ID, &correct);
	
	return res;
}

static inline errno_t obj40_check_unix_ext(obj40_t *obj, 
					   obj40_stat_methods_t *methods,
					   obj40_stat_params_t *params, 
					   uint8_t mode)
{
	reiser4_place_t *stat = &obj->info.start;
	sdext_unix_hint_t hint, correct;
	errno_t res;
	int fixed;

	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_read_ext(stat, SDEXT_UNIX_ID, &hint)) < 0)
		return res;
	
	correct = hint;

	if (methods->check_bytes == NULL) {
		/* Call the default one. */
		fixed = obj40_check_bytes(obj, &correct.bytes, params->bytes);
	} else if (methods->check_bytes != SKIP_METHOD) {
		fixed = methods->check_bytes(obj, &correct.bytes, 
					     params->bytes);
	} else {
		fixed = 0;
	}

	if (fixed) {
		/* sd_bytes are set wrongly in the kernel. */
		aal_error("Node (%llu), item (%u): StatData of the file [%s] "
			  "has the wrong bytes (%llu), %s (%llu). Plugin (%s).",
			  stat->node->block->nr, stat->pos.item, 
			  print_inode(obj->core, &stat->key),
			  hint.bytes, mode == RM_CHECK ? "Should be" : 
			  "Fixed to", correct.bytes, stat->plug->label);
		
		/* Zero rdev because rdev and bytes is the union on disk
		   but not in the unix_hint. */
		correct.rdev = 0;
		res = RE_FIXABLE;
	}

	
	if (res && mode != RM_CHECK)
		return obj40_write_ext(stat, SDEXT_UNIX_ID, &correct);

	return res;
}

errno_t obj40_check_stat(obj40_t *obj, /*uint64_t must_exts, uint64_t unkn_exts,*/
			 obj40_stat_methods_t *methods, 
			 obj40_stat_params_t *params,
			 /*nlink_func_t nlink_func, mode_func_t mode_func, 
			 size_func_t size_func, uint64_t size, 
			 uint64_t bytes, */ uint8_t mode)
{
	reiser4_place_t *stat;
	uint64_t extmask;
	errno_t res;
	
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
	
	if ((extmask = obj40_extmask(stat)) == MAX_UINT64) {
		aal_error("Node (%llu), item (%u), plugin (%s): failed "
			  "to obtain the StatData extention mask.",
			  stat->node->block->nr, stat->pos.item,
			  stat->plug->label);
		return res;
	}
	
	/* Check the LW extension. */
	if ((res = obj40_check_lw_ext(obj, methods, params, mode)))
		return res;
	
	/* Check the UNIX extention. */
	return obj40_check_unix_ext(obj, methods, params, mode);
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

errno_t obj40_prepare_stat(obj40_t *obj, uint16_t objmode, uint8_t mode) {
	reiser4_place_t *start;
	trans_hint_t trans;
	reiser4_key_t *key;
	lookup_t lookup;
	uint64_t pid;
	errno_t res;

	aal_assert("vpf-1225", obj != NULL);
	
	start = STAT_PLACE(obj);
	key = &obj->info.object;

	/* Update the place of SD. */
	if ((lookup = obj40_find_item(obj, key, FIND_EXACT, 
				      NULL, NULL, start)) < 0)
		return lookup;

	if (lookup == PRESENT) {
		/* Check if SD item is found. */
		if (start->plug->id.group == STATDATA_ITEM)
			return 0;
		
		/* Not SD item is found. Possible only when a fake
		   object was created. */
		aal_error("Node (%llu), item (%u), %s: not "
			  "StatData is found by the key (%s).%s",
			  start->node->block->nr, start->pos.item, 
			  start->plug->label, print_key(obj->core, key),
			  mode == RM_BUILD ? "Removed." : "");

		if (mode != RM_BUILD)
			return RE_FATAL;

		aal_memset(&trans, 0, sizeof(trans));
		trans.shift_flags = SF_DEFAULT;
		trans.count = 1;

		if ((res = obj40_remove(obj, start, &trans)))
			return res;
	}

	/* SD is absent. Create a new one. 
	   THIS IS THE SPECIAL CASE and usually is not used as object plugin 
	   cannot be recognized w/out SD. Used for for "/" and "lost+found" 
	   recovery. */
	
	aal_error("The file [%s] does not have a StatData item.%s Plugin "
		  "%s.", print_inode(obj->core, key), mode == RM_BUILD ? 
		  " Creating a new one." : "",  obj->plug->label);

	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((pid = obj->core->param_ops.value("statdata") == INVAL_PID))
		return -EINVAL;

	if ((res = obj40_create_stat(obj, pid, 0, 0, 0, 0, objmode, 
				     objmode == S_IFLNK ? "FAKE_LINK" : NULL)))
	{
		aal_error("The file [%s] failed to create a "
			  "StatData item. Plugin %s.", 
			  print_inode(obj->core, key),
			  obj->plug->label);
	}

	return res;
}
#endif
