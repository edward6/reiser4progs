/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifndef ENABLE_STAND_ALONE

#include "dir40_repair.h"

/* Set of extentions that must present. */
#define DIR40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID)

/* Set of unknown extentions. */
#define DIR40_EXTS_UNKN ((uint64_t)1 << SDEXT_SYMLINK_ID)

object_entity_t *dir40_recognize(object_info_t *info) {
	dir40_t *dir;
	errno_t res;
	
	aal_assert("vpf-1231", info != NULL);
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, info, dir40_core);
	
	if ((res = obj40_objkey_check(&dir->obj)))
		goto error;

	if ((res = obj40_check_stat(&dir->obj, DIR40_EXTS_MUST,
				    DIR40_EXTS_UNKN)))
		goto error;
	
	/* Positioning to the first directory unit */
	dir40_reset((object_entity_t *)dir);
	
	return (object_entity_t *)dir;
 error:
	aal_free(dir);
	return res < 0 ? INVAL_PTR : NULL;
}

static errno_t dir40_dot(dir40_t *dir, reiser4_plug_t *bplug, uint8_t mode) {
	object_info_t *info;
	entry_hint_t entry;
	trans_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-1242", dir != NULL);
	aal_assert("vpf-1244", bplug != NULL);
	
	/* Lookup the "." */
	if ((res = dir40_reset((object_entity_t *)dir)))
		return res;
	
	if ((res = obj40_find_item(&dir->obj, &dir->position,  
				   FIND_CONV, NULL, NULL,
				   &dir->body)) < 0)
	{
		return res;
	}

	if (res == PRESENT)
		return 0;
	
	info = &dir->obj.info;
	
	aal_error("Directory [%s]: The entry \".\" is not found.%s "
		  "Plugin (%s).", print_inode(dir40_core, &info->object), 
		  mode != RM_CHECK ? " Insert a new one." : "", 
		  dir->obj.info.opset.plug[OPSET_OBJ]->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	/* Absent. Add a new ".". Take it from the stat_hint for now.

	   FIXME-VITALY: It can be stored in SD also, but it is not clear 
	   under which type -- ITEM_PLUG? Fix it when reiser4 syscall will 
	   be ready. */
		
	aal_memset(&hint, 0, sizeof(hint));

	hint.count = 1;
	hint.plug = bplug;
	hint.shift_flags = SF_DEFAULT;
	
	aal_memcpy(&hint.offset, &dir->position, sizeof(dir->position));
	aal_memcpy(&entry.offset,  &dir->position, sizeof(dir->position));
	aal_memcpy(&entry.object,  &dir->obj.info.object, sizeof(entry.object));

	aal_strncpy(entry.name, ".", 1);
	hint.specific = &entry;
	res = obj40_insert(&dir->obj, &dir->body, &hint, LEAF_LEVEL);

	return res < 0 ? res : 0;
}

errno_t dir40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	dir40_t *dir = (dir40_t *)object;
	obj40_stat_hint_t hint;
	obj40_stat_ops_t ops;
	object_info_t *info;
	entry_hint_t entry;
	
	errno_t res;
	
	aal_assert("vpf-1224", dir != NULL);
	aal_assert("vpf-1190", dir->obj.info.tree != NULL);
	aal_assert("vpf-1197", dir->obj.info.object.plug != NULL);
	
	info = &dir->obj.info;

	aal_memset(&ops, 0, sizeof(ops));
	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_prepare_stat(&dir->obj, S_IFDIR, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&info->start, data))
		return -EINVAL;
	
	/* Take care about the ".". */
	/* FIXME: Probably it should be different -- find an item by the key 
	   and if it is of DIR group, take its plugin as body plug, fix 
	   it in SD then. */
	if ((res |= dir40_dot(dir, object->opset.plug[OPSET_DIRITEM], 
			      mode)) < 0)
		return res;
	
	/* FIXME-VITALY: this probably should be changed. Now hash plug that is
	   used is taken from SD or the default one from @hint. Probably it
	   would be better to do evth in vise versa order -- choose the hash
	   found among the entries most of the times and correct hash plugin in
	   SD. */
	while (1) {
		pos_t *pos = &dir->body.pos;
		trans_hint_t trans;
		reiser4_key_t key;
		lookup_t lookup;
		uint32_t units;
		
		if ((lookup = dir40_update_body(object, 0)) < 0) 
			return lookup;

		/* No more items of the dir40. */
		if (lookup == ABSENT)
			break;
		
		/* Looks like an item of dir40. If there were some key collisions, 
		   this search was performed with incremented adjust, decrement it 
		   here. */
		if (dir->position.adjust)
			dir->position.adjust--;
			
		/* Item can be of another plugin, but of the same group. 
		   FIXME-VITALY: item of the same group but of another 
		   plugin, it should be converted. */
		/*if (dir->body.plug->id.group != DIR_ITEM) */
		if (dir->body.plug != object->opset.plug[OPSET_DIRITEM]) {
			aal_error("Directory [%s], plugin [%s], node [%llu], "
				  "item [%u]: item of the illegal plugin [%s] "
				  "with the key of this object found.%s",
				  print_inode(dir40_core, &info->object),
				  dir40_plug.label, place_blknr(&dir->body),
				  dir->body.pos.item, dir->body.plug->label,
				  mode == RM_BUILD ? " Removed." : "");

			if (mode != RM_BUILD)
				return RE_FATAL;
			
			trans.count = 1;
			trans.shift_flags = SF_DEFAULT & ~SF_ALLOW_PACK;
			pos->unit = MAX_UINT32;

			/* Item has wrong key, remove it. */
			res |= obj40_remove(&dir->obj, &dir->body, &trans);
			if (res < 0) return res;
			
			continue;
		}

		if (pos->unit == MAX_UINT32)
			pos->unit = 0;
		
		units = plug_call(dir->body.plug->o.item_ops->balance, 
				  units, &dir->body);
		
		for (; pos->unit < units; pos->unit++) {
			bool_t last = (pos->unit == units - 1);
			
			if (last) {
				/* If we are handling the last unit, register 
				   the item despite the result of handling.
				   Any item has a pointer to objectid in the 
				   key, if it is shared between 2 objects, it
				   should be already solved at relocation time.
				 */
				if (place_func && place_func(&dir->body, data))
					return -EINVAL;

				/* Count size and bytes. */
				hint.size += plug_call(dir->body.plug->o.item_ops->object,
						       size, &dir->body);

				hint.bytes += plug_call(dir->body.plug->o.item_ops->object,
							bytes, &dir->body);

			}
			
			if ((res |= dir40_fetch(dir, &entry)) < 0)
				return res;
			
			/* Prepare the correct key for the entry. */
			plug_call(entry.offset.plug->o.key_ops, 
				  build_hashed, &key,
				  object->opset.plug[OPSET_HASH], 
				  object->opset.plug[OPSET_FIBRE], 
				  obj40_locality(&dir->obj),
				  obj40_objectid(&dir->obj), entry.name);
			
			/* If the key matches, continue. */
			if (!plug_call(key.plug->o.key_ops, compfull, 
				       &key, &entry.offset))
				goto next;
			
			/* Broken entry found, remove it. */
			aal_error("Directory [%s], plugin [%s], node [%llu], "
				  "item [%u], unit [%u]: entry has wrong "
				  "offset [%s]. Should be [%s].%s", 
				  print_inode(dir40_core, &info->object),
				  dir40_plug.label, place_blknr(&dir->body),
				  dir->body.pos.item, dir->body.pos.unit,
				  print_key(dir40_core, &entry.offset),
				  print_key(dir40_core, &key), 
				  mode == RM_BUILD ? " Removed." : "");


			if (mode != RM_BUILD) {
				/* If not the BUILD mode, continue with the 
				   entry key, not the correct one. */
				plug_call(key.plug->o.key_ops, assign,
					  &key, &entry.offset);
				res |= RE_FATAL;
				goto next;
			}

			trans.count = 1;
			trans.shift_flags = SF_DEFAULT & ~SF_ALLOW_PACK;

			if ((res |= obj40_remove(&dir->obj, &dir->body, 
						 &trans)) < 0)
				return res;
			
			/* Update accounting info after remove. */
			if (last) {
				hint.size--;
				hint.bytes -= trans.bytes;
			}
			
			/* Lookup it again. */
			break;
			
		next:
			/* The key is ok. */
			if (plug_call(key.plug->o.key_ops, compfull, 
				      &dir->position, &key))
			{
				/* Key differs from the offset of the 
				   last left entry. */
				plug_call(key.plug->o.key_ops, assign,
					  &dir->position, &key);
			} else if (aal_strlen(entry.name) != 1 ||
				   aal_strncmp(entry.name, ".", 1))
			{
				/* Key collision. */
				dir->position.adjust++;
			}
		}
		
		/* Lookup for the last entry left in the tree with the 
		   incremented adjust to get the next one. */
		dir->position.adjust++;
	}
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		hint.mode = S_IFDIR;
		hint.nlink = 1;
		hint.must_exts = DIR40_EXTS_MUST;
		hint.unkn_exts = DIR40_EXTS_UNKN;
		ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;

		res |= obj40_update_stat(&dir->obj, &ops, &hint, mode);
	}
	
	dir40_reset((object_entity_t *)dir);

	return res;
}

errno_t dir40_check_attach(object_entity_t *object, 
			   object_entity_t *parent,
			   place_func_t place_func, 
			   void *data, uint8_t mode)
{
	dir40_t *dir = (dir40_t *)object;
	lookup_t lookup;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	lookup = dir40_lookup(object, "..", &entry);
	entry.place_func = place_func;
	entry.data = data;
	
	switch (lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!plug_call(entry.object.plug->o.key_ops, compfull, 
			       &entry.object, &parent->object))
			break;
		
		/* Already attached. */
		aal_error("Directory [%s], plugin [%s]: the object "
			  "is attached already to [%s] and cannot "
			  "be attached to [%s].", 
			  print_inode(dir40_core, &object->object),
			  dir40_plug.label, 
			  print_key(dir40_core, &entry.object),
			  print_inode(dir40_core, &parent->object));

		return RE_FATAL;
	case ABSENT:
		/* Not attached yet. */
/*
		if (plug_call(object->info.object.plug->o.key_ops, compfull,
			      &object->info.object, &parent->info.object))
		{
			aal_error("Directory [%s], plugin [%s]: the "
			"object is not attached. %s [%s].",
			print_inode(dir40_core, &object->info.object),
			dir40_plug.label, mode == RM_CHECK ? 
			"Reached from" : "Attaching to",
			print_inode(dir40_core, &parent->info.object));
		}
*/		
		if (mode == RM_CHECK) {
			aal_error("Directory [%s], plugin [%s]: the object "
				  "is not attached. Reached from [%s].",
				  print_inode(dir40_core, &object->object),
				  dir40_plug.label,
				  print_inode(dir40_core, &parent->object));
			return RE_FIXABLE;
		}
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, 
			  assign, &entry.object, &parent->object);

		aal_strncpy(entry.name, "..", sizeof(entry.name));
		
		if ((res = plug_call(object->opset.plug[OPSET_OBJ]->o.object_ops,
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
	
	return plug_call(parent->opset.plug[OPSET_OBJ]->o.object_ops, 
			 link, parent);
}


/* Creates the fake dir40 entity by the given @info for the futher recovery. */
object_entity_t *dir40_fake(object_info_t *info) {
	dir40_t *dir;
	
	aal_assert("vpf-1231", info != NULL);
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, info, dir40_core);
	
	/* Positioning to the first directory unit */
	dir40_reset((object_entity_t *)dir);
	
	return (object_entity_t *)dir;
}

#endif
