/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */

#ifndef ENABLE_STAND_ALONE

#include "obj40_repair.h"

static errno_t obj40_extentions_check(reiser4_place_t *stat, 
				      uint64_t exts_must, 
				      uint64_t exts_unkn) 
{
	uint64_t extmask;
	
	aal_assert("vpf-1623", stat != NULL);
	
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extension. */
	if (extmask & exts_unkn)
		return RE_FATAL;
	
	/* Check that LW and UNIX extensions exist. */
	return ((extmask & exts_must) == exts_must) ? 0 : RE_FATAL;
}

/* Checks that @obj->info.start is SD of the wanted file.  */
errno_t obj40_check_stat(obj40_t *obj, uint64_t exts_must, uint64_t exts_unkn) {
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
	return obj40_extentions_check(&info->start, exts_must, exts_unkn);
}

/* The plugin tries to recognize the object: detects the SD, body items */
errno_t obj40_objkey_check(obj40_t *obj) {
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
	return plug_call(info->object.plug->o.key_ops, compfull, 
			 &key, &info->object) ? RE_FATAL : 0;
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
					 uint8_t mode, int present)
{
	reiser4_place_t *start = &obj->info.start;
	sdext_lw_hint_t hint, correct;
	errno_t res;
	int fixed;

	aal_memset(&hint, 0, sizeof(hint));
	
	/* If the LW extention is not present, skip checking or add it. */
	if (!present) {
		trans_hint_t trans;
		statdata_hint_t stat;
		
		/* If the LW extention is not mandatory, skip checking. */
		if (!(params->must_exts & (1 << SDEXT_LW_ID)))
			return 0;
		
		aal_error("Node (%llu), item (%u), plugin (%s): StatData of the"
			  " file [%s] does not have a mandatory light-weight "
			  "extention.%s Plugin (%s).", place_blknr(start), 
			  start->pos.item, start->plug->label,
			  print_inode(obj->core, &start->key),
			  mode != RM_CHECK ? "Added." : "", 
			  obj->info.opset[OPSET_OBJ]->label);

		if (mode == RM_CHECK)
			return RE_FIXABLE;
		
		/* Add missed, mandatory extention. */
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.shift_flags = SF_DEFAULT;
		trans.specific = &stat;
		trans.plug = start->plug;
		
		hint.size = params->size;
		hint.nlink = params->nlink;
		hint.mode = params->mode | 0755;

		stat.ext[SDEXT_LW_ID] = &hint;
		stat.extmask = (1 << SDEXT_LW_ID);

		start->pos.unit = 0;

		if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
			return res;

		if ((res = obj40_update(obj)))
			return res;

		/* Do not return here, but check the new extention with the 
		   given set of methods instead. */
	}
	
	/* Read LW extension. */
	if ((res = obj40_read_ext(start, SDEXT_LW_ID, &hint)))
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
		aal_error("Node (%llu), item (%u): StatData of the "
			  "file [%s] has the wrong mode (%u), %s (%u).",
			  place_blknr(start), start->pos.item,
			  print_inode(obj->core, &start->key), hint.mode,
			  mode == RM_CHECK ? "Should be" : "Fixed to", 
			  correct.mode);

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
		aal_error("Node (%llu), item (%u): StatData of the file "
			  "[%s] has the wrong size (%llu), %s (%llu).",
			  place_blknr(start), start->pos.item, 
			  print_inode(obj->core, &start->key), hint.size, 
			  mode == RM_CHECK ? "Should be" : "Fixed to", 
			  correct.size);

		res = RE_FIXABLE;
	}

	if (res && mode != RM_CHECK)
		return obj40_write_ext(start, SDEXT_LW_ID, &correct);
	
	return res;
}

static inline errno_t obj40_check_unix_ext(obj40_t *obj, 
					   obj40_stat_methods_t *methods,
					   obj40_stat_params_t *params, 
					   uint8_t mode, int present)
{
	reiser4_place_t *start = &obj->info.start;
	sdext_unix_hint_t hint, correct;
	errno_t res;
	int fixed;

	aal_memset(&hint, 0, sizeof(hint));
	
	/* If the UNIX extention is not present, skip checking or add it. */
	if (!present) {
		trans_hint_t trans;
		statdata_hint_t stat;
		
		/* If the LW extention is not mandatory, skip checking. */
		if (!(params->must_exts & (1 << SDEXT_UNIX_ID)))
			return 0;
		
		aal_error("Node (%llu), item (%u), plugin (%s): StatData "
			  "of the file [%s] does not have a mandatory unix "
			  "extention.%s Plugin (%s).", place_blknr(start), 
			  start->pos.item, start->plug->label,
			  print_inode(obj->core, &start->key),
			  mode != RM_CHECK ? " Added." : "", 
			  obj->info.opset[OPSET_OBJ]->label);

		if (mode == RM_CHECK)
			return RE_FIXABLE;

		/* Add missed, mandatory extention. */
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.shift_flags = SF_DEFAULT;
		trans.specific = &stat;
		trans.plug = start->plug;
		
		hint.rdev = 0;
		hint.bytes = params->bytes;
		
		hint.uid = getuid();
		hint.gid = getgid();
		hint.atime = time(NULL);
		hint.mtime = hint.atime;
		hint.ctime = hint.atime;

		stat.ext[SDEXT_UNIX_ID] = &hint;
		stat.extmask = (1 << SDEXT_UNIX_ID);

		start->pos.unit = 0;
		
		if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
			return res;

		if ((res = obj40_update(obj)))
			return res;

		/* Do not return here, but check the new extention with the 
		   given set of methods instead. */
	}

	if ((res = obj40_read_ext(start, SDEXT_UNIX_ID, &hint)) < 0)
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
		aal_error("Node (%llu), item (%u): StatData of the "
			  "file [%s] has the wrong bytes (%llu), %s "
			  "(%llu).", place_blknr(start), start->pos.item,
			  print_inode(obj->core, &start->key), hint.bytes, 
			  mode == RM_CHECK ? "Should be" : "Fixed to", 
			  correct.bytes);
		
		/* Zero rdev because rdev and bytes is the union on disk
		   but not in the unix_hint. */
		correct.rdev = 0;
		res = RE_FIXABLE;
	}

	
	if (res && mode != RM_CHECK)
		return obj40_write_ext(start, SDEXT_UNIX_ID, &correct);

	return res;
}

/* Check the set of SD extentions and their contents. */
errno_t obj40_update_stat(obj40_t *obj,
			  obj40_stat_methods_t *methods, 
			  obj40_stat_params_t *params,
			  uint8_t mode)
{
	reiser4_place_t *start;
	uint64_t extmask;
	errno_t res;
	
	aal_assert("vpf-1213", obj != NULL);
	
	start = &obj->info.start;
	
	/* Update the SD place. */
	if ((res = obj40_update(obj)))
		return res;
	
	/* Get the set of present SD extentions. */
	if ((extmask = obj40_extmask(start)) == MAX_UINT64) {
		aal_error("Node (%llu), item (%u), plugin (%s): failed "
			  "to obtain the StatData extention mask.",
			  place_blknr(start), start->pos.item,
			  start->plug->label);
		return res;
	}

	/* Remove unknown SD extentions. */
	if (extmask & params->unkn_exts) {
		trans_hint_t trans;
		statdata_hint_t stat;
		
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		stat.extmask = extmask & params->unkn_exts;
		
		aal_error("Node (%llu), item (%u): StatData of the "
			  "file [%s] has some unknown extentions "
			  "(mask=%llu).%s Plugin (%s).", place_blknr(start),
			  start->pos.item, print_inode(obj->core, &start->key),
			  stat.extmask, mode != RM_CHECK ? " Removed." : "", 
			  start->plug->label);

		if (mode != RM_CHECK) {
			trans.specific = &stat;
			trans.shift_flags = SF_DEFAULT;
			start->pos.unit = 0;
			trans.count = 0;

			if ((res = obj40_remove(obj, start, &trans)))
				return res;

			/* Update the SD place. */
			if ((res = obj40_update(obj)))
				return res;
		}
	}

	/* Check the LW extension. */
	if ((res = obj40_check_lw_ext(obj, methods, params, mode,
				      extmask & (1 << SDEXT_LW_ID))) < 0)
		return res;

	/* Check the UNIX extention. */
	res |= obj40_check_unix_ext(obj, methods, params, mode,
				    extmask & (1 << SDEXT_UNIX_ID));

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
	
	aal_error("Node (%llu), item (%u), plugin (%s): the key [%s] of the "
		  "item is wrong, %s [%s]. Plugin (%s).", place_blknr(place),
		  place->pos.unit, place->plug->label, 
		  print_key(obj->core, &place->key), mode == RM_BUILD ? 
		  "fixed to" : "should be", print_key(obj->core, key), 
		  obj->info.opset[OPSET_OBJ]->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	if ((res = obj->core->tree_ops.update_key(obj->info.tree,
						  place, key)))
	{
		aal_error("Node (%llu), item(%u): update of the "
			  "item key failed.", place_blknr(place),
			  place->pos.unit);
	}

	return res;
}

errno_t obj40_prepare_stat(obj40_t *obj, uint16_t objmode, uint8_t mode) {
	reiser4_place_t *start;
	trans_hint_t trans;
	reiser4_key_t *key;
	lookup_t lookup;
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
		aal_error("Node (%llu), item (%u), plugin (%s): not "
			  "StatData is found by the key (%s).%s",
			  place_blknr(start), start->pos.item, 
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
	
	aal_error("The file [%s] does not have a StatData item.%s Plugin %s.",
		  print_inode(obj->core, key), mode == RM_BUILD ? " Creating "
		  "a new one." : "",  obj->info.opset[OPSET_OBJ]->label);

	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((res = obj40_create_stat(obj, 0, 0, 0, 0, objmode, 
				     objmode == S_IFLNK ? "FAKE_LINK" : NULL)))
	{
		aal_error("The file [%s] failed to create a StatData item. "
			  "Plugin %s.", print_inode(obj->core, key),
			  obj->info.opset[OPSET_OBJ]->label);
	}

	return res;
}
#endif
