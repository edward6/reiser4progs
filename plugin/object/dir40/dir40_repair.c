/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifndef ENABLE_STAND_ALONE
#include "dir40.h"
#include "repair/plugin.h"

#define dir40_exts ((uint64_t)1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID)

static errno_t dir40_extensions(reiser4_place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extension. */
	/*
	if (extmask & ~(dir40_exts | 1 << SDEXT_PLUG_ID))
		return RE_FATAL;
	*/
	/* Check that LW and UNIX extensions exist. */
	return ((extmask & dir40_exts) == dir40_exts) ? 0 : RE_FATAL;
}

/* Check SD extensions and that mode in LW extension is DIRFILE. */
static errno_t callback_stat(reiser4_place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = dir40_extensions(stat)))
		return res;

	/* Check the mode in the LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)))
		return res;
	
	return S_ISDIR(lw_hint.mode) ? 0 : RE_FATAL;
}

object_entity_t *dir40_recognize(object_info_t *info) {
	dir40_t *dir;
	errno_t res;
	
	aal_assert("vpf-1231", info != NULL);
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plug, dir40_core, info);
	
	if ((res = obj40_recognize(&dir->obj, callback_stat)))
		goto error;

	/* Positioning to the first directory unit */
	dir40_reset((object_entity_t *)dir);
	
	return (object_entity_t *)dir;
 error:
	aal_free(dir);
	return res < 0 ? INVAL_PTR : NULL;
}

static void dir40_one_nlink(obj40_t *obj, uint32_t *nlink) {
	*nlink = 1;
}

static void dir40_check_mode(obj40_t *obj, uint16_t *mode) {
	if (!S_ISDIR(*mode)) {
		*mode &= ~S_IFMT;
        	*mode |= S_IFDIR;
	}
}

static void dir40_check_size(obj40_t *obj, uint64_t *sd_size, uint64_t counted_size) {
	if (*sd_size != counted_size)
		*sd_size = counted_size;
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
	
	if ((res = obj40_lookup(&dir->obj, &dir->position, LEAF_LEVEL, 
				FIND_EXACT, NULL, NULL, &dir->body)) < 0)
	{
		return res;
	}

	if (res == PRESENT)
		return 0;
	
	info = &dir->obj.info;
	
	aal_error("Directory [%s]: The entry \".\" is not found.%s "
		  "Plugin (%s).", print_inode(dir40_core, &info->object), 
		  mode != RM_CHECK ? " Inserts a new one." : "", 
		  dir->obj.plug->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	/* Absent. Add a new ".". Take it from the param for now.

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

#if 0
static errno_t dir40_belongs(dir40_t *dir) {
	aal_assert("vpf-1245", dir != NULL);
	
	/* Check that the body place is valid. */
	if (!dir40_core->tree_ops.valid(dir->obj.info.tree, &dir->body))
		return RE_FATAL;

	/* Fetching item info at @place */
	if (obj40_fetch(&dir->obj, &dir->body))
		return -EINVAL;

	/* Does the body item belong to the current object. */
	return plug_call(dir->body.key.plug->o.key_ops, compshort,
			 &dir->body.key, &dir->position) ? RE_FATAL : 0;
}
#endif

/* Search for the position for the read in the directory by @dir->position,
   Returns ABSENT only if there is no more entries in the directory. */
static lookup_t dir40_search(dir40_t *dir) {
	uint32_t units, adjust;
	lookup_t res;
	
	aal_assert("vpf-1343", dir != NULL);
	
	adjust = dir->position.adjust;
	
	/* Making tree_lookup() to find entry by key */
	if ((res = obj40_lookup(&dir->obj, &dir->position, LEAF_LEVEL, 
				FIND_EXACT, NULL, NULL, &dir->body)) < 0)
		return res;
	
	/* No adjusting for the ABSENT result. */
	if (res == ABSENT) adjust = 0;
	
	units = plug_call(dir->body.plug->o.item_ops->balance,
			  units, &dir->body);

	if (dir->body.pos.unit == MAX_UINT32)
		dir->body.pos.unit = 0;
	
	do {
		entry_hint_t temp;
		
		if (dir->body.pos.unit >= units) {
			/* Getting next directory item */
			if ((res = dir40_next(dir)) < 0)
				return res;
			
			/* No more items in the tree. */
			if (res == ABSENT) return ABSENT;

			/* Some item of the dir was found. */
			if (!adjust) return PRESENT;
				
			units = plug_call(dir->body.plug->o.item_ops->balance,
					  units, &dir->body);

		} else if (!adjust)
			/* We get here from above with PRESENT only. */
			return PRESENT;
				
		if (dir40_fetch(dir, &temp))
			return -EIO;

		if (plug_call(temp.offset.plug->o.key_ops, compfull, 
			      &temp.offset, &dir->position))
		{
			/* Greater key is reached. */
			return PRESENT;
		}

		adjust--;
		dir->body.pos.unit++;
	} while (adjust ||  dir->body.pos.unit >= units);

	return PRESENT;	
}

errno_t dir40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	dir40_t *dir = (dir40_t *)object;
	reiser4_plug_t *bplug;
	object_info_t *info;
	entry_hint_t entry;
	rid_t pid;
	
	uint64_t size, bytes;
	errno_t res;
	
	aal_assert("vpf-1224", dir != NULL);
	aal_assert("vpf-1190", dir->obj.info.tree != NULL);
	aal_assert("vpf-1197", dir->obj.info.object.plug != NULL);
	
	info = &dir->obj.info;
	
	if ((res = obj40_launch_stat(&dir->obj, NULL, dir40_exts, 1, 
				     S_IFDIR, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_fix_key(&dir->obj, &info->start, 
				 &info->object, mode)) < 0)
		return res;
	
	/* Init hash plugin in use. */
	dir->hash = obj40_plug_recognize(&dir->obj, HASH_PLUG_TYPE, "hash");
	
	if (dir->hash == NULL) {
                aal_error("Directory %s: failed to init hash plugin."
			  "Plugin (%s).", print_inode(dir40_core, &info->object),
			  dir40_plug.label);
                return -EINVAL;
        }
	
	/* FIXME-VITALY: take it from SD first. But of which type -- there is 
	   only ITEM_TYPE for now. */
	if ((pid = dir40_core->param_ops.value("direntry")) == INVAL_PID) {
		aal_error("Failed to get a plugid for direntry from "
			  "the params.");
		return -EINVAL;
	}
	
	if ((bplug = dir40_core->factory_ops.ifind(ITEM_PLUG_TYPE, pid)) == NULL) {
		aal_error("Failed to find direntry plugin by "
			  "the id %d.", pid);
                 return -EINVAL;
	}

	/* Take care about the ".". */
	if ((res |= dir40_dot(dir, bplug, mode)) < 0)
		return res;
	
	size = 0; bytes = 0; 
	
	/* FIXME-VITALY: this probably should be changed. Now hash plug that is
	   used is taken from SD or the default one from the params. Probably it
	   would be better to do evth in vise versa order -- choose the hash
	   found among the entries most of the times and correct hash plugin in
	   SD. */
	while (1) {
		pos_t *pos = &dir->body.pos;
		trans_hint_t hint;
		reiser4_key_t key;
		lookup_t lookup;
		uint32_t units;
		
		if ((lookup = dir40_search(dir)) < 0) 
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
		/*if (dir->body.plug->id.group != DIRENTRY_ITEM) {*/
		if (dir->body.plug != bplug) {
			aal_error("Directory [%s], plugin [%s], node [%llu], "
				  "item [%u]: item of the illegal plugin [%s] "
				  "with the key of this object found.%s",
				  print_inode(dir40_core, &info->object),
				  dir40_plug.label, dir->body.node->block->nr,
				  dir->body.pos.item, dir->body.plug->label,
				  mode == RM_BUILD ? " Removed." : "");

			if (mode == RM_BUILD)
				return RE_FATAL;
			
			hint.count = 1;
			hint.shift_flags = SF_DEFAULT;

			/* Item has wrong key, remove it. */
			res |= obj40_remove(&dir->obj, &dir->body, &hint);
			if (res < 0) return res;
			
			continue;
		}

		
		units = plug_call(dir->body.plug->o.item_ops->balance, 
				  units, &dir->body);
		
		for (; pos->unit < units; pos->unit++) {
			if ((res |= dir40_fetch(dir, &entry)) < 0)
				return res;
			
			/* Prepare the correct key for the entry. */
			plug_call(entry.offset.plug->o.key_ops, build_hashed, 
				  &key, dir->hash, obj40_locality(&dir->obj),
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
				  dir40_plug.label, dir->body.node->block->nr,
				  dir->body.pos.item, dir->body.pos.unit,
				  print_key(dir40_core, &entry.offset),
				  print_key(dir40_core, &key), 
				  mode == RM_BUILD ? " Removed." : "");


			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				goto next;
			}

			hint.count = 1;

			res |= obj40_remove(&dir->obj, &dir->body, &hint);
			if (res < 0) return res;

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
		
		if (pos->unit == units) {
			/* Try to register the item if it has not been yet. Any 
			   item has a pointer to objectid in the key, if it is 
			   shared between 2 objects, it should be already solved 
			   at relocation time. */
			if (place_func && place_func(&dir->body, data))
				return -EINVAL;

			/* Count size and bytes. */
			size += plug_call(dir->body.plug->o.item_ops->object, 
					  size, &dir->body);

			bytes += plug_call(dir->body.plug->o.item_ops->object, 
					   bytes, &dir->body);
		}
		
		/* Lookup for the last entry left in the tree with the 
		   incremented adjust to get the next one. */
		dir->position.adjust++;
	}
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		res |= obj40_check_stat(&dir->obj, mode == RM_BUILD ?
					dir40_one_nlink : NULL,
					dir40_check_mode,
					dir40_check_size, 
					size, bytes, mode);
	}
	
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
			       &entry.object, &parent->info.object))
			break;
		
		/* Already attached. */
		aal_error("Directory [%s], plugin [%s]: the object "
			  "is attached already to [%s] and cannot "
			  "be attached to [%s].", 
			  print_inode(dir40_core, &object->info.object),
			  dir40_plug.label, print_key(dir40_core, &entry.object),
			  print_inode(dir40_core, &parent->info.object));

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
				  print_inode(dir40_core, &object->info.object),
				  dir40_plug.label,
				  print_inode(dir40_core, &parent->info.object));
			return RE_FIXABLE;
		}
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, assign,
			  &entry.object, &parent->info.object);

		aal_strncpy(entry.name, "..", sizeof(entry.name));
		
		if ((res = plug_call(object->plug->o.object_ops,
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
	
	return plug_call(parent->plug->o.object_ops, link, parent);
}


/* Creates the fake dir40 entity by the given @info for the futher recovery. */
object_entity_t *dir40_fake(object_info_t *info) {
	dir40_t *dir;
	
	aal_assert("vpf-1231", info != NULL);
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plug, dir40_core, info);
	
	/* Positioning to the first directory unit */
	dir40_reset((object_entity_t *)dir);
	
	return (object_entity_t *)dir;
}

/* Form the correct object from what was recognzed and checked: for now
   @info.parent, dir->hash, If ".." cannot be found, zero the key. */
errno_t dir40_form(object_entity_t *object) {
	dir40_t *dir = (dir40_t *)object;
	entry_hint_t entry;

	aal_assert("vpf-1269", object != NULL);

	dir40_reset(object);

	/* Init hash plugin in use if is not known yet. */
	if (!dir->hash) {
		dir->hash = obj40_plug_recognize(&dir->obj, 
						 HASH_PLUG_TYPE, 
						 "hash");

		if (dir->hash == NULL) {
			aal_error("Directory [%s]: failed to init "
				  "hash plugin. Plugin (%s).", 
				  print_inode(dir40_core, &dir->obj.info.object),
				  dir40_plug.label);
			return -EINVAL;
		}
	}
	
	switch ((dir40_lookup(object, "..", &entry))) {
	case ABSENT:
		aal_memset(&object->info.parent, 0, 
			   sizeof(object->info.parent));
		break;
	case PRESENT:
		plug_call(entry.object.plug->o.key_ops, assign,
			  &object->info.parent, &entry.object);
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

#endif
