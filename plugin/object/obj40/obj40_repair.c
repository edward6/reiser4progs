/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */

#ifndef ENABLE_STAND_ALONE

#include "obj40_repair.h"

static errno_t obj40_exts_check(reiser4_place_t *stat, 
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
	
	if (info->start.plug->id.group != STAT_ITEM)
		return RE_FATAL;
	
	/* Compare the correct key with the place key. */
	if (plug_call(info->object.plug->o.key_ops, compfull,
		      &info->object, &info->start.key))
		return RE_FATAL;
	
	/* Some SD is recognized. Check that this is our SD. */
	return obj40_exts_check(&info->start, exts_must, exts_unkn);
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

static inline errno_t obj40_check_lw(obj40_t *obj, 
				     obj40_stat_ops_t *ops,
				     obj40_stat_hint_t *hint, 
				     uint8_t mode, int present)
{
	reiser4_place_t *start = &obj->info.start;
	sdhint_lw_t lwh, correct;
	errno_t res;
	int fixed;

	aal_memset(&lwh, 0, sizeof(lwh));
	
	/* If the LW extention is not present, skip checking or add it. */
	if (!present) {
		trans_hint_t trans;
		stat_hint_t stat;
		
		/* If the LW extention is not mandatory, skip checking. */
		if (!(hint->must_exts & (1 << SDEXT_LW_ID)))
			return 0;
		
		aal_error("Node (%llu), item (%u), plugin (%s): StatData of the"
			  " file [%s] does not have a mandatory light-weight "
			  "extention.%s Plugin (%s).", place_blknr(start), 
			  start->pos.item, start->plug->label,
			  print_inode(obj->core, &start->key),
			  mode != RM_CHECK ? "Added." : "", 
			  obj->info.opset.plug[OPSET_OBJ]->label);

		if (mode == RM_CHECK)
			return RE_FIXABLE;
		
		/* Add missed, mandatory extention. */
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.shift_flags = SF_DEFAULT;
		trans.specific = &stat;
		trans.plug = start->plug;
		
		lwh.size = hint->size;
		lwh.nlink = hint->nlink;
		lwh.mode = hint->mode | 0755;

		stat.ext[SDEXT_LW_ID] = &lwh;
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
	if ((res = obj40_read_ext(start, SDEXT_LW_ID, &lwh)))
		return res;

	/* Form the correct LW extension. */
	correct = lwh;

	if (ops->check_nlink == NULL) {
		/* Call the default one. */
		fixed = obj40_check_nlink(obj, &correct.nlink, hint->nlink);
	} else if (ops->check_nlink != SKIP_METHOD) {
		fixed = ops->check_nlink(obj, &correct.nlink, hint->nlink);
	} else {
		fixed = 0;
	}

	if (fixed) {
		/* Fix nlink silently: there is no way to check if nlink is 
		   correct, so the check is either skipped or rebuild. */
		res = RE_FIXABLE;
	}

	if (ops->check_mode == NULL) {
		/* Call the default one. */
		fixed = obj40_check_mode(obj, &correct.mode, hint->mode);
	} else if (ops->check_mode != SKIP_METHOD) {
		fixed = ops->check_mode(obj, &correct.mode, hint->mode);
	} else {
		fixed = 0;
	}

	if (fixed) {
		aal_error("Node (%llu), item (%u): StatData of the "
			  "file [%s] has the wrong mode (%u), %s (%u).",
			  place_blknr(start), start->pos.item,
			  print_inode(obj->core, &start->key), lwh.mode,
			  mode == RM_CHECK ? "Should be" : "Fixed to", 
			  correct.mode);

		res = RE_FIXABLE;
	}

	if (ops->check_size == NULL) {
		/* Call the default one. */
		fixed = obj40_check_size(obj, &correct.size, hint->size);
	} else if (ops->check_size != SKIP_METHOD) {
		fixed = ops->check_size(obj, &correct.size, hint->size);
	} else {
		fixed = 0;
	}

	if (fixed) {
		aal_error("Node (%llu), item (%u): StatData of the file "
			  "[%s] has the wrong size (%llu), %s (%llu).",
			  place_blknr(start), start->pos.item, 
			  print_inode(obj->core, &start->key), lwh.size, 
			  mode == RM_CHECK ? "Should be" : "Fixed to", 
			  correct.size);

		res = RE_FIXABLE;
	}

	if (res && mode != RM_CHECK)
		return obj40_write_ext(start, SDEXT_LW_ID, &correct);
	
	return res;
}

static inline errno_t obj40_check_unix(obj40_t *obj, 
				       obj40_stat_ops_t *ops,
				       obj40_stat_hint_t *hint, 
				       uint8_t mode, int present)
{
	reiser4_place_t *start = &obj->info.start;
	sdhint_unix_t unixh, correct;
	errno_t res;
	int fixed;

	aal_memset(&unixh, 0, sizeof(unixh));
	
	/* If the UNIX extention is not present, skip checking or add it. */
	if (!present) {
		trans_hint_t trans;
		stat_hint_t stat;
		
		/* If the LW extention is not mandatory, skip checking. */
		if (!(hint->must_exts & (1 << SDEXT_UNIX_ID)))
			return 0;
		
		aal_error("Node (%llu), item (%u), plugin (%s): StatData "
			  "of the file [%s] does not have a mandatory unix "
			  "extention.%s Plugin (%s).", place_blknr(start), 
			  start->pos.item, start->plug->label,
			  print_inode(obj->core, &start->key),
			  mode != RM_CHECK ? " Added." : "", 
			  obj->info.opset.plug[OPSET_OBJ]->label);

		if (mode == RM_CHECK)
			return RE_FIXABLE;

		/* Add missed, mandatory extention. */
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.shift_flags = SF_DEFAULT;
		trans.specific = &stat;
		trans.plug = start->plug;
		
		unixh.rdev = 0;
		unixh.bytes = hint->bytes;
		
		unixh.uid = getuid();
		unixh.gid = getgid();
		unixh.atime = time(NULL);
		unixh.mtime = unixh.atime;
		unixh.ctime = unixh.atime;

		stat.ext[SDEXT_UNIX_ID] = &unixh;
		stat.extmask = (1 << SDEXT_UNIX_ID);

		start->pos.unit = 0;
		
		if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
			return res;

		if ((res = obj40_update(obj)))
			return res;

		/* Do not return here, but check the new extention with the 
		   given set of methods instead. */
	}

	if ((res = obj40_read_ext(start, SDEXT_UNIX_ID, &unixh)) < 0)
		return res;
	
	correct = unixh;

	if (ops->check_bytes == NULL) {
		/* Call the default one. */
		fixed = obj40_check_bytes(obj, &correct.bytes, hint->bytes);
	} else if (ops->check_bytes != SKIP_METHOD) {
		fixed = ops->check_bytes(obj, &correct.bytes, hint->bytes);
	} else {
		fixed = 0;
	}

	if (fixed) {
		/* sd_bytes are set wrongly in the kernel. */
		aal_error("Node (%llu), item (%u): StatData of the "
			  "file [%s] has the wrong bytes (%llu), %s "
			  "(%llu).", place_blknr(start), start->pos.item,
			  print_inode(obj->core, &start->key), unixh.bytes, 
			  mode == RM_CHECK ? "Should be" : "Fixed to", 
			  correct.bytes);
		
		/* Zero rdev because rdev and bytes is the union on disk
		   but not in the unixh. */
		correct.rdev = 0;
		res = RE_FIXABLE;
	}

	
	if (res && mode != RM_CHECK)
		return obj40_write_ext(start, SDEXT_UNIX_ID, &correct);

	return res;
}

/*
   This is not yet clear how to detect the correct plugin, e.g. formatting,
   and figure out if it is essential or not and leave detected or fix evth 
   to the one from SD. So evth is recovered according to SD plugins, except 
   essential ones. Understanding that this pset member is non-essensial is 
   hardcoded yet. If this willl be changed, use obj40_check_plug */

#if 0
static inline errno_t obj40_check_plug(obj40_t *obj, 
				       obj40_stat_ops_t *ops,
				       obj40_stat_hint_t *hint, 
				       uint8_t mode, int present)
{
	reiser4_place_t *start;
	trans_hint_t trans;
	stat_hint_t stat;
	uint8_t i, diff;
	errno_t res;

	aal_assert("vpf-1650", obj->info.opset.mask == hint->plugh.mask);
	
	/* Get plugins that must exists in the PLUGID extention. */
	obj->core->pset_ops.diff(obj->info.tree, &hint->plugh);

	start = &obj->info.start;
	
	for (i = 0, diff = 0; i < OPSET_LAST; i++) {
		if (!hint->plugh.plug[i]) {
			/* Leave all present on-disk pset members. */
			if (aal_test_bit(&obj->info.opset.mask, i)) {
				hint->plugh.plug[i] = obj->info.opset.plug[i];
				aal_set_bit(&hint->plugh.mask, i);
			}

			continue;
		}
		
		if (hint->plugh.plug[i] == INVAL_PTR) {
			/* Remove all wrongly present on-disk pset members. */
			aal_error("Node (%llu), item (%u), plugin (%s): "
				  "StatData of the file [%s] (%s) has the "
				  "essential slot (%u) of the plugin extention "
				  "set to (%s) that matches the fs-default one."
				  "%s Removed.", place_blknr(start),
				  start->pos.item, start->plug->label,
				  print_inode(obj->core, &start->key),
				  obj->info.opset.plug[OPSET_OBJ]->label,
				  i, obj->info.opset.plug[i]->label,
				  mode == RM_CHECK ? " Should be" : "");

			hint->plugh.plug[i] = 0;

			diff++;
			
			continue;
		}
		
		if (hint->plugh.plug[i] != obj->info.opset.plug[i]) {
			/* Fix wrong plugins in pset members. */
			aal_error("Node (%llu), item (%u), plugin (%s): "
				  "StatData of the file [%s] (%s) has the "
				  "essential slot (%u) of the plugin extention "
				  "set to (%s), %s (%s).", place_blknr(start),
				  start->pos.item, start->plug->label,
				  print_inode(obj->core, &start->key),
				  obj->info.opset.plug[OPSET_OBJ]->label,
				  i, obj->info.opset.plug[i]->label,
				  mode == RM_CHECK ? " Should be" : 
				  "Fixed to", hint->plugh.plug[i]->label);

			diff++;
		}
	}
	
	if (!diff || mode == RM_CHECK) return 0;

	/* Prepare hints. For removing & adding plug extention. */
	aal_memset(&trans, 0, sizeof(trans));
	aal_memset(&stat, 0, sizeof(stat));
	
	trans.shift_flags = SF_DEFAULT;
	trans.specific = &stat;
	trans.plug = start->plug;
	start->pos.unit = 0;

	stat.extmask = (1 << SDEXT_PLUG_ID);

	if (obj->info.opset.mask) {
		/* Plug extention is the SD is wrong. Remove it first. */
		
		if ((res = obj40_remove(obj, start, &trans)))
			return res;
		
		if ((res = obj40_update(obj)))
			return res;
	}
	
	stat.ext[SDEXT_PLUG_ID] = &hint->plugh;
	
	if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
		return res;

	return obj40_update(obj);
}

#endif
	
/* Check the set of SD extentions and their contents. */
errno_t obj40_update_stat(obj40_t *obj, obj40_stat_ops_t *ops,
			  obj40_stat_hint_t *hint, uint8_t mode)
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
	if (extmask & hint->unkn_exts) {
		trans_hint_t trans;
		stat_hint_t stat;
		
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		stat.extmask = extmask & hint->unkn_exts;
		
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
	if ((res = obj40_check_lw(obj, ops, hint, mode,
				  extmask & (1 << SDEXT_LW_ID))) < 0)
		return res;

	/* Check the UNIX extention. */
	res |= obj40_check_unix(obj, ops, hint, mode, 
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
		  print_key(obj->core, &place->key), mode == RM_CHECK ? 
		  "should be" : "fixed to", print_key(obj->core, key), 
		  obj->info.opset.plug[OPSET_OBJ]->label);
	
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
		if (start->plug->id.group == STAT_ITEM)
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
		  "a new one." : "",  obj->info.opset.plug[OPSET_OBJ]->label);

	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((res = obj40_create_stat(obj, 0, 0, 0, 0, objmode, 
				     objmode == S_IFLNK ? "FAKE_LINK" : NULL)))
	{
		aal_error("The file [%s] failed to create a StatData item. "
			  "Plugin %s.", print_inode(obj->core, key),
			  obj->info.opset.plug[OPSET_OBJ]->label);
	}

	return res;
}
#endif
