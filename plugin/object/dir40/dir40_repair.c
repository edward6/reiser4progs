/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifndef ENABLE_MINIMAL

#include "dir40_repair.h"

static errno_t dir40_dot(reiser4_object_t *dir, 
			 reiser4_item_plug_t *bplug, 
			 uint8_t mode) 
{
	object_info_t *info;
	entry_hint_t entry;
	trans_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-1242", dir != NULL);
	aal_assert("vpf-1244", bplug != NULL);
	
	/* Lookup the "." */
	if ((res = dir40_reset(dir)))
		return res;
	
	if ((res = obj40_find_item(dir, &dir->position, FIND_CONV, 
				   NULL, NULL, &dir->body)) < 0)
	{
		return res;
	}

	if (res == PRESENT)
		return 0;
	
	info = &dir->info;
	
	fsck_mess("Directory [%s]: The entry \".\" is not found.%s "
		  "Plugin (%s).", print_inode(obj40_core, &info->object), 
		  mode != RM_CHECK ? " Insert a new one." : "", 
		  reiser4_psobj(dir)->p.label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	/* Absent. Add a new ".". */
		
	aal_memset(&hint, 0, sizeof(hint));

	hint.count = 1;
	hint.plug = bplug;
	hint.shift_flags = SF_DEFAULT;
	
	aal_memcpy(&hint.offset, &dir->position, sizeof(dir->position));
	aal_memcpy(&entry.offset,  &dir->position, sizeof(dir->position));
	aal_memcpy(&entry.object,  &dir->info.object, sizeof(entry.object));

	aal_strncpy(entry.name, ".", 1);
	hint.specific = &entry;
	res = obj40_insert(dir, &dir->body, &hint, LEAF_LEVEL);

	return res < 0 ? res : 0;
}

static errno_t dir40_entry_check(reiser4_object_t *dir,
				 obj40_stat_hint_t *hint,
				 place_func_t func, 
				 void *data, 
				 uint8_t mode)
{
	entry_hint_t entry;
	trans_hint_t trans;
	reiser4_key_t key;
	uint32_t units;
	errno_t result;
	errno_t res;
	bool_t last;
	pos_t *pos;

	res = 0;
	pos = &dir->body.pos;
	units = objcall(&dir->body, balance->units);
	
	if (pos->unit == MAX_UINT32)
		pos->unit = 0;
	
	last = 0;
	for (; pos->unit < units; pos->unit++) {
		last = (pos->unit == units - 1);
		
		if (last) {
			/* If we are handling the last unit, register the item 
			   despite the result of handling. Any item has a 
			   pointer to objectid in the key, if it is shared 
			   between 2 objects, it should be already solved at 
			   relocation time. */
			if (func && func(&dir->body, data))
				return -EINVAL;

			/* Count size and bytes. */
			hint->size += objcall(&dir->body, object->size);
			hint->bytes += objcall(&dir->body, object->bytes);
		}

		if ((result = dir40_fetch(dir, &entry)) < 0)
			return result;

		/* Prepare the correct key for the entry. */
		plugcall(entry.offset.plug, build_hashed, &key,
			 reiser4_pshash(dir), reiser4_psfibre(dir),
			 objcall(&dir->info.object, get_locality),
			 objcall(&dir->info.object, get_objectid), entry.name);

		/* If the key matches, continue. */
		if (objcall(&key, compfull, &entry.offset)) {
			/* Broken entry found, remove it. */
			fsck_mess("Directory [%s] (%s), node [%llu], "
				  "item [%u], unit [%u]: entry has wrong "
				  "offset [%s]. Should be [%s].%s", 
				  print_inode(obj40_core, &dir->info.object),
				  reiser4_psobj(dir)->p.label, 
				  (unsigned long long)place_blknr(&dir->body), 
				  dir->body.pos.item, dir->body.pos.unit,
				  print_key(obj40_core, &entry.offset),
				  print_key(obj40_core, &key), 
				  mode == RM_BUILD ? " Removed." : "");
			
			if (mode != RM_BUILD) {
				/* If not the BUILD mode, continue with the 
				   entry key, not the correct one. */
				aal_memcpy(&key, &entry.offset, sizeof(key));
				res |= RE_FATAL;
			} else {
				break;
			}
		}

		/* Either key is ok or we are in CHECK mode, take the next 
		   entry. */
		if (objcall(&dir->position, compfull, &key)) {
			/* Key differs from the last left entry offset. */
			aal_memcpy(&dir->position, &key, sizeof(key));
		} else if (aal_strlen(entry.name) != 1 ||
			   aal_strncmp(entry.name, ".", 1))
		{
			/* Key collision. */
			dir->position.adjust++;
		}

	}
	
	/* All entries were handled. */
	if (pos->unit == units)
		return res;
	
	/* Some entry is bad and needs to be removed. */
	trans.count = 1;
	trans.shift_flags = SF_DEFAULT & ~SF_ALLOW_PACK;

	if ((result = obj40_remove(dir, &dir->body, &trans)) < 0)
		return result;
	
	/* Adjust position to the following incrementing, needed as the entry 
	   is removed. */
	dir->position.adjust--;

	/* Update accounting info after remove. */
	if (last) {
		hint->size--;
		hint->bytes -= trans.bytes;
	}

	return 0;
}

static errno_t dir40_check_item(reiser4_object_t *dir, void *data) {
	uint8_t mode = *(uint8_t *)data;
	
	/* FIXME-VITALY: item of the same group but of another plugin,
	   should it be converted? */
	if (dir->body.plug != reiser4_psdiren(dir)) {
		fsck_mess("Directory [%s] (%s), node [%llu], item"
			  "[%u]: item of the illegal plugin (%s) "
			  "with the key of this object found.%s",
			  print_inode(obj40_core, &dir->info.object),
			  reiser4_psobj(dir)->p.label, 
			  (unsigned long long)place_blknr(&dir->body),
			  dir->body.pos.item, dir->body.plug->p.label,
			  mode == RM_BUILD ? " Removed." : "");

		return mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	}
	return 0;
}

errno_t dir40_check_struct(reiser4_object_t *dir,
			   place_func_t func,
			   void *data, uint8_t mode)
{
	obj40_stat_hint_t hint;
	object_info_t *info;
	
	errno_t res;
	
	aal_assert("vpf-1224", dir != NULL);
	aal_assert("vpf-1190", dir->info.tree != NULL);
	aal_assert("vpf-1197", dir->info.object.plug != NULL);
	
	info = &dir->info;

	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_prepare_stat(dir, S_IFDIR, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (func && func(&info->start, data))
		return -EINVAL;
	
	/* Take care about the ".". */
	/* FIXME: Probably it should be different -- find an item by the key 
	   and if it is of DIR group, take its plugin as body plug, fix 
	   it in SD then. */
	if ((res |= dir40_dot(dir, reiser4_psdiren(dir), mode)) < 0)
		return res;
	
	while (1) {
		lookup_t lookup;
		
		lookup = obj40_check_item(dir, dir40_check_item, 
					  dir40_entry_comp, &mode);
		
		if (repair_error_fatal(lookup))
			return lookup;
		else if (lookup == ABSENT)
			break;
		
		/* Looks like an item of dir40. If there were some key collisions, 
		   this search was performed with incremented adjust, decrement it 
		   here. */
		if (dir->position.adjust)
			dir->position.adjust--;

		if ((res |= dir40_entry_check(dir, &hint, func, 
					      data, mode)) < 0)
		{
			return res;
		}
		
		/* Lookup for the last handled entry key with the incremented 
		   adjust to get the next entry. */
		dir->position.adjust++;
	}
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		obj40_stat_ops_t ops;
		
		aal_memset(&ops, 0, sizeof(ops));
		ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
		
		hint.mode = S_IFDIR;
		hint.nlink = 1;

		res |= obj40_update_stat(dir, &ops, &hint, mode);
	}
	
	dir40_reset(dir);

	return res;
}

errno_t dir40_check_attach(reiser4_object_t *object, 
			   reiser4_object_t *parent,
			   place_func_t func, 
			   void *data, uint8_t mode)
{
	entry_hint_t entry;
	lookup_t lookup;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	lookup = dir40_lookup(object, "..", &entry);
	entry.place_func = func;
	entry.data = data;
	
	switch (lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!objcall(&entry.object, compfull, &parent->info.object))
			break;
		
		/* Already attached. */
		fsck_mess("Directory [%s] (%s): the object "
			  "is attached already to [%s] and cannot "
			  "be attached to [%s].", 
			  print_inode(obj40_core, &object->info.object),
			  reiser4_psobj(object)->p.label, 
			  print_key(obj40_core, &entry.object),
			  print_inode(obj40_core, &parent->info.object));

		return RE_FATAL;
	case ABSENT:
		/* Not attached yet. */
/*
		if (plug_call(object->info.object.plug, compfull,
			      &object->info.object, &parent->info.object))
		{
			fsck_mess("Directory [%s] (%s): the "
			"object is not attached. %s [%s].",
			print_inode(obj40_core, &object->info.object),
			reiser4_psobj(object)->p.label, mode == RM_CHECK ? 
			"Reached from" : "Attaching to",
			print_inode(obj40_core, &parent->info.object));
		}
*/		
		if (mode == RM_CHECK) {
			fsck_mess("Directory [%s] (%s): the object "
				  "is not attached. Reached from [%s].",
				  print_inode(obj40_core, &object->info.object),
				  reiser4_psobj(object)->p.label,
				  print_inode(obj40_core, &parent->info.object));
			return RE_FIXABLE;
		}
		
		/* Adding ".." to the @object pointing to the @parent. */
		aal_memcpy(&entry.object, &parent->info.object, 
			   sizeof(entry.offset));

		aal_strncpy(entry.name, "..", sizeof(entry.name));
		
		if ((res = plugcall(reiser4_psobj(object), 
				    add_entry, object, &entry)))
		{
			return res;
		}

		break;
	default:
		return lookup;
	}

	/* ".." matches the parent. Now do parent->nlink++ for REBUILD mode. */
	if (mode != RM_BUILD)
		return 0;
	
	return plugcall(reiser4_psobj(parent), link, parent);
}


/* Creates the fake dir40 entity by the given @info for the futher recovery. */
errno_t dir40_fake(reiser4_object_t *dir) {
	aal_assert("vpf-1231", dir != NULL);
	
	/* Positioning to the first directory unit */
	dir40_reset(dir);
	
	return 0;
}

#endif
