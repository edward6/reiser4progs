/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.c -- reiser4 file 40 plugins repair code. */

#ifndef ENABLE_MINIMAL

#include "obj40_repair.h"

#define STAT_KEY(o) \
        (&((o)->info.start.key))

static void obj40_init(reiser4_object_t *object) {
	if (!object->info.start.plug)
		aal_memcpy(STAT_KEY(object), &object->info.object, 
			   sizeof(object->info.object));
}

/* Obtains the maxreal key of the given place. */
uint64_t obj40_place_maxreal(reiser4_place_t *place) {
	reiser4_key_t key;
	objcall(place, balance->maxreal_key, &key);
	return objcall(&key, get_offset);
}

static errno_t obj40_exts_check(reiser4_object_t *obj) {
	reiser4_object_plug_t *ops;
	uint64_t extmask;
	
	aal_assert("vpf-1831", obj != NULL);
	
	extmask = obj40_extmask(&obj->info.start);
	
	ops = reiser4_psobj(obj);
	
	/* Check that there is no one unknown extension. */
	if (extmask & ops->sdext_unknown)
		return RE_FATAL;
	
	/* Check that LW and UNIX extensions exist. */
	return ((extmask & ops->sdext_mandatory) == 
		ops->sdext_mandatory) ? 0 : RE_FATAL;
}

/* Checks that @obj->info.start is SD of the wanted file.  */
static errno_t obj40_check_stat(reiser4_object_t *obj) {
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
	
	if (info->start.plug->p.id.group != STAT_ITEM)
		return RE_FATAL;
#if 0	
	/* Compare the correct key with the place key. */
	if (plug_call(info->object.plug, compfull,
		      &info->object, &info->start.key))
	{
		return RE_FATAL;
	}
#endif	
	/* Some SD is recognized. Check that this is our SD. */
	return obj40_exts_check(obj);
}

/* The plugin tries to recognize the object: detects the SD, body items */
static errno_t obj40_objkey_check(reiser4_object_t *obj) {
	object_info_t *info;
	reiser4_key_t key;

	aal_assert("vpf-1121", obj->info.tree != NULL);
	aal_assert("vpf-1127", obj->info.object.plug != NULL);
	
	info = &obj->info;
	
	/* Check if the key pointer is correct and then check the found item 
	   if it is SD with the proper key. */
	plugcall(info->object.plug, 
		 build_generic, &key, KEY_STATDATA_TYPE,
		 objcall(&info->object, get_locality),
		 objcall(&info->object, get_ordering),
		 objcall(&info->object, get_objectid), 0);

	/* Compare the correct key with the search one. */
	return objcall(&key, compfull, &info->object) ? RE_FATAL : 0;
}

errno_t obj40_recognize(reiser4_object_t *obj) {
	errno_t res;
	
	aal_assert("vpf-1231", obj != NULL);
	
	/* Initializing file handle */
	obj40_init(obj);
	
	if ((res = obj40_objkey_check(obj)))
		return res;

	if ((res = obj40_check_stat(obj)))
		return res;
	
	/* Positioning to the first directory unit */
	if (reiser4_psobj(obj)->reset)
		reiser4_psobj(obj)->reset(obj);
	
	return 0;
}

#define OBJ40_CHECK(field, type, value, correct)			\
static inline int obj40_check_##field(reiser4_object_t *obj,			\
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

static inline void obj40_check_bytes_report(reiser4_place_t *start,
					    reiser4_core_t *core,
					    uint64_t found_bytes,
					    uint8_t mode,
					    uint64_t correct_bytes)
{
	fsck_mess("Node (%llu), item (%u), [%s] (%s): wrong bytes "
		  "(%llu), %s (%llu).",
		  (unsigned long long)place_blknr(start),
		  start->pos.item, print_inode(core, &start->key),
		  start->plug->p.label,
		  (unsigned long long)found_bytes,
		  mode == RM_CHECK ? "Should be" : "Fixed to",
		  (unsigned long long)correct_bytes);
}

static inline int obj40_check_mode(reiser4_object_t *obj, 
				   uint16_t *mode, 
				   uint16_t correct) 
{
	if (((*mode) & S_IFMT) == correct)
		return 0;

	*mode &= ~S_IFMT;
	*mode |= correct;
	return 1;
}

static inline errno_t obj40_stat_unix_check(reiser4_object_t *obj, 
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
		if (!(reiser4_psobj(obj)->sdext_mandatory & 
		      (1 << SDEXT_UNIX_ID)))
		{
			return 0;
		}
		
		fsck_mess("Node (%llu), item (%u), [%s] (%s): no mandatory "
			  "unix extention.%s Plugin (%s).",
			  (unsigned long long)place_blknr(start),
			  start->pos.item, print_inode(obj40_core, &start->key),
			  start->plug->p.label, mode != RM_CHECK ? 
			  " Added." : "", reiser4_psobj(obj)->p.label);

		if (mode == RM_CHECK)
			return RE_FIXABLE;

		/* Add missed, mandatory extention. */
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.shift_flags = SF_DEFAULT;
		trans.specific = &stat;
		trans.plug = start->plug;
		
		obj40_stat_unix_init(&stat, &unixh, hint->bytes, 0);
		
		start->pos.unit = 0;
		
		if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
			return res;

		if ((res = obj40_update(obj)))
			return res;

		/* Do not return here, but check the new extention with the 
		   given set of methods instead. */
	}

	if ((res = obj40_read_ext(obj, SDEXT_UNIX_ID, &unixh)) < 0)
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
		if (ops->check_bytes_report == NULL) {
			/* Report with the default method */
			obj40_check_bytes_report(start,
						 obj40_core,
						 unixh.bytes,
						 mode,
						 correct.bytes);
		} else if (ops->check_bytes_report != SKIP_METHOD) {
			ops->check_bytes_report(start,
						obj40_core,
						unixh.bytes,
						mode,
						correct.bytes);
		} else {
			;
		}
		/* Zero rdev because rdev and bytes is the union on disk
		   but not in the unixh. */
		correct.rdev = 0;
		res = RE_FIXABLE;
	}

	
	if (res && mode != RM_CHECK)
		return obj40_write_ext(obj, SDEXT_UNIX_ID, &correct);

	return res;
}

static inline errno_t obj40_stat_lw_check(reiser4_object_t *obj, 
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
		if (!(reiser4_psobj(obj)->sdext_mandatory &
		      (1 << SDEXT_LW_ID)))
		{
			return 0;
		}
		fsck_mess("Node (%llu), item (%u), [%s] (%s): no mandatory "
			  "light-weight extention.%s Plugin (%s).",
			  (unsigned long long)place_blknr(start),
			  start->pos.item, 
			  print_inode(obj40_core, &start->key),
			  start->plug->p.label, mode != RM_CHECK ? 
			  "Added." : "", reiser4_psobj(obj)->p.label);

		if (mode == RM_CHECK)
			return RE_FIXABLE;
		
		/* Add missed, mandatory extention. */
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.shift_flags = SF_DEFAULT;
		trans.specific = &stat;
		trans.plug = start->plug;
		
		obj40_stat_lw_init(obj, &stat, &lwh, hint->size, 
				   hint->nlink, hint->mode);

		start->pos.unit = 0;

		if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
			return res;

		if ((res = obj40_update(obj)))
			return res;

		/* Do not return here, but check the new extention with the 
		   given set of methods instead. */
	}
	
	/* Read LW extension. */
	if ((res = obj40_read_ext(obj, SDEXT_LW_ID, &lwh)))
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
		fsck_mess("Node (%llu), item (%u), [%s] (%s): wrong mode (%u), "
			  "%s (%u).",
			  (unsigned long long)place_blknr(start),
			  start->pos.item,
			  print_inode(obj40_core, &start->key),
			  start->plug->p.label, lwh.mode, mode == RM_CHECK ?
			  "Should be" : "Fixed to", correct.mode);

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
		fsck_mess("Node (%llu), item (%u), [%s] (%s): wrong size (%llu)"
			  ", %s (%llu).",
			  (unsigned long long)place_blknr(start),
			  start->pos.item,
			  print_inode(obj40_core, &start->key),
			  start->plug->p.label,
			  (unsigned long long)lwh.size,
			  mode == RM_CHECK ? "Should be" : "Fixed to",
			  (unsigned long long)correct.size);

		res = RE_FIXABLE;
	}

	if (res && mode != RM_CHECK)
		return obj40_write_ext(obj, SDEXT_LW_ID, &correct);
	
	return res;
}

/*
   This is not yet clear how to detect the correct plugin, e.g. formatting,
   and figure out if it is essential or not and leave detected or fix evth 
   to the one from SD. So evth is recovered according to SD plugins, except 
   essential ones. The knowledge if a pset member is essensial is hardcoded 
   yet. 
   
   obj40_check_plug is used to form the on-disk pset according to already
   existent SD pset and tree->pset. Actually used for the root only. */

static inline errno_t obj40_stat_pset_check(reiser4_object_t *obj, 
					    uint8_t mode, int present) 
{
	reiser4_place_t *start;
	sdhint_plug_t plugh;
	trans_hint_t trans;
	stat_hint_t stat;
	uint64_t diff;
	uint64_t mask;
	errno_t res;

	aal_assert("vpf-1650", obj != NULL);
	
	start = &obj->info.start;
	
	/* Get plugins that must exists in the PLUGID extention. */
	mask = obj40_core->pset_ops.build_mask(obj->info.tree, &obj->info.pset);
	mask &= ((1 << PSET_STORE_LAST) - 1);
	
	if ((diff = (mask != obj->info.pset.plug_mask))) {
		fsck_mess("Node (%llu), item (%u), [%s] (%s): wrong plugin "
			  "set is stored on disk (0x%llx). %s (0x%llx).",
			  (unsigned long long)place_blknr(start),
			  start->pos.item,
			  print_inode(obj40_core, &start->key),
			  start->plug->p.label,
			  (unsigned long long)obj->info.pset.plug_mask,
			  mode == RM_CHECK ? "Should be" : "Fixed to",
			  (unsigned long long)mask);
	}
	if (!diff || mode == RM_CHECK)
		return diff ? RE_FIXABLE : 0;

	/* Prepare hints. For removing & adding plug extention. */
	aal_memset(&trans, 0, sizeof(trans));
	aal_memset(&stat, 0, sizeof(stat));
	
	trans.shift_flags = SF_DEFAULT;
	trans.specific = &stat;
	trans.plug = start->plug;
	start->pos.unit = 0;
	stat.extmask = (1 << SDEXT_PSET_ID);

	if (present) {
		/* Plug extention is the SD is wrong. Remove it first. */
		if ((res = obj40_remove(obj, start, &trans)))
			return res;
		
		if ((res = obj40_update(obj)))
			return res;

		if ((res = obj40_fetch_item(&obj->info.start)))
			return res;
	}
	
	obj->info.pset.plug_mask = mask;
	
	/* Pass plugh there instead of obj->info.pset to not get the 
	   altered result after the modification */
	aal_memcpy(&plugh, &obj->info.pset, sizeof(plugh));
	stat.ext[SDEXT_PSET_ID] = &plugh;
	
	start->pos.unit = 0;
	
	if ((res = obj40_insert(obj, start, &trans, LEAF_LEVEL)))
		return res;

	return obj40_update(obj);
}

/* Check the set of SD extentions and their contents. */
errno_t obj40_update_stat(reiser4_object_t *obj, obj40_stat_ops_t *ops,
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
		aal_error("Node (%llu), item (%u), (%s): failed "
			  "to obtain the StatData extention mask.",
			  (unsigned long long)place_blknr(start),
			  start->pos.item,
			  start->plug->p.label);
		return -EIO;
	}

	/* Remove unknown SD extentions. */
	if (extmask & reiser4_psobj(obj)->sdext_unknown) {
		trans_hint_t trans;
		stat_hint_t stat;
		
		aal_memset(&trans, 0, sizeof(trans));
		aal_memset(&stat, 0, sizeof(stat));
		
		stat.extmask = extmask & reiser4_psobj(obj)->sdext_unknown;
		
		fsck_mess("Node (%llu), item (%u), [%s]: StatData has some "
			  "unknown extentions (mask=%llu).%s Plugin (%s).",
			  (unsigned long long)place_blknr(start),
			  start->pos.item,
			  print_inode(obj40_core, &start->key),
			  (unsigned long long)stat.extmask,
			  mode != RM_CHECK ? " Removed." : "",
			  start->plug->p.label);

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

			if ((res = obj40_fetch_item(&obj->info.start)))
				return res;
		}
	}
	
	/* Check the UNIX extention. */
	if ((res = obj40_stat_unix_check(obj, ops, hint, mode,
					 extmask & (1 << SDEXT_UNIX_ID))) < 0)
	{
		return res;
	}
	
	/* Check the LW extension. */
	if ((res |= obj40_stat_lw_check(obj, ops, hint, mode, 
					extmask & (1 << SDEXT_LW_ID))) < 0)
	{
		return res;
	}
	
	/* Check the Plugin SET extention. */
	if ((res |= obj40_stat_pset_check(obj, mode, 
					  extmask & (1 << SDEXT_PSET_ID))) < 0)
	{
		return res;
	}
	
	/* The sdext_sym & sdext_crc are either mandatory or unknown.
	   Nothing to check there. */
	
	return res;
}

errno_t obj40_prepare_stat(reiser4_object_t *obj, uint16_t objmode, uint8_t mode) {
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
		if (start->plug->p.id.group == STAT_ITEM)
			return 0;
		
		/* Not SD item is found. Possible only when a fake
		   object was created. */
		fsck_mess("Node (%llu), item (%u), (%s): not "
			  "StatData is found by the key (%s).%s",
			  (unsigned long long)place_blknr(start),
			  start->pos.item, 
			  start->plug->p.label, print_key(obj40_core, key),
			  mode == RM_BUILD ? "Removed." : "");

		if (mode != RM_BUILD)
			return RE_FATAL;

		aal_memset(&trans, 0, sizeof(trans));
		trans.shift_flags = SF_DEFAULT;
		trans.count = 1;
		start->pos.unit = MAX_UINT32;

		if ((res = obj40_remove(obj, start, &trans)))
			return res;
	}

	/* SD is absent. Create a new one. 
	   THIS IS THE SPECIAL CASE and usually is not used as object plugin 
	   cannot be recognized w/out SD. Used for for "/" and "lost+found" 
	   recovery only. */
	
	fsck_mess("The file [%s] does not have a StatData item.%s Plugin %s.",
		  print_inode(obj40_core, key), mode == RM_BUILD ? " Creating "
		  "a new one." : "",  reiser4_psobj(obj)->p.label);

	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((res = obj40_create_stat(obj, 0, 0, 0, 0, objmode, 
				     objmode == S_IFLNK ? "FAKE_LINK" : NULL)))
	{
		aal_error("The file [%s] failed to create a StatData item. "
			  "Plugin %s.", print_inode(obj40_core, key),
			  reiser4_psobj(obj)->p.label);
	}

	return res;
}

lookup_t obj40_check_item(reiser4_object_t *obj, 
			  obj_func_t item_func,
			  obj_func_t update_func,
			  void *data)
{
	trans_hint_t trans;
	errno_t res;
		
	while (1) {
		res = obj40_update_body(obj, update_func);

		if (res != PRESENT && res != -ESTRUCT)
			return res;
		
		if ((res = item_func(obj, data)) && res != -ESTRUCT)
			return res;
		
		if (res == 0)
			return PRESENT;
		
		aal_memset(&trans, 0, sizeof(trans));
		
		trans.count = 1;
		trans.shift_flags = SF_DEFAULT & ~SF_ALLOW_PACK;
		obj->body.pos.unit = MAX_UINT32;
		
		/* Item has wrong key, remove it. */
		if ((res = obj40_remove(obj, &obj->body, &trans)) < 0)
			return res;
	}
}

#endif

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
