/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifndef ENABLE_STAND_ALONE
#include "dir40.h"
#include "repair/plugin.h"

extern reiser4_core_t *dcore;
extern reiser4_plug_t dir40_plug;

extern errno_t dir40_reset(object_entity_t *entity);
extern lookup_t dir40_lookup(object_entity_t *entity,
			     char *name, entry_hint_t *entry);

extern errno_t dir40_fetch(object_entity_t *entity, entry_hint_t *entry);

#define dir40_exts ((uint64_t)1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID)

static errno_t dir40_extentions(place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extention. */
	/*
	if (extmask & ~(dir40_exts | 1 << SDEXT_PLUG_ID))
		return RE_FATAL;
	*/
	/* Check that LW and UNIX extentions exist. */
	return ((extmask & dir40_exts) == dir40_exts) ? 0 : RE_FATAL;
}

/* Check SD extentions and that mode in LW extention is DIRFILE. */
static errno_t callback_stat(place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = dir40_extentions(stat)))
		return res;

	/* Check the mode in the LW extention. */
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
	obj40_init(&dir->obj, &dir40_plug, dcore, info);
	
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
	trans_hint_t body_hint;
	object_info_t *info;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1242", dir != NULL);
	aal_assert("vpf-1244", bplug != NULL);
	
	/* Lookup the "." */
	if ((res = dir40_reset((object_entity_t *)dir)))
		return res;
	
	if ((res = obj40_lookup(&dir->obj, &dir->offset, LEAF_LEVEL, 
				FIND_EXACT, &dir->body)) < 0)
		return res;

	if (res == PRESENT)
		return 0;
	
	info = &dir->obj.info;
	
	aal_exception_error("Directory [%s]: The entry \".\" is not found.%s "
			    "Plugin (%s).", print_ino(dcore, &info->object), 
			    mode != RM_CHECK ? " Inserts a new one." : "", 
			    dir->obj.plug->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	/* Absent. Add a new ".". Take it from the param for now.

	   FIXME-VITALY: It can be stored in SD also, but it is not clear under
	   which type -- ITEM_PLUG? Fix it when reiser4 syscall will be
	   ready. */
		
	aal_memset(&body_hint, 0, sizeof(body_hint));
	
	body_hint.count = 1;
	body_hint.plug = bplug;
	
	aal_memcpy(&body_hint.offset, &dir->offset, sizeof(dir->offset));
	aal_memcpy(&entry.offset,  &dir->offset, sizeof(dir->offset));
	aal_memcpy(&entry.object,  &dir->obj.info.object, sizeof(entry.object));
	
	aal_strncpy(entry.name, ".", 1);
	body_hint.specific = &entry;
	return obj40_insert(&dir->obj, &dir->body, &body_hint, LEAF_LEVEL);
}

static errno_t dir40_belongs(dir40_t *dir, reiser4_plug_t *bplug) {
	aal_assert("vpf-1245", dir != NULL);
	aal_assert("vpf-1246", bplug != NULL);
	
	/* Check that the body place is valid. */
	if (!dcore->tree_ops.valid(dir->obj.info.tree, &dir->body))
		return RE_FATAL;

	/* Fetching item info at @place */
	if (obj40_fetch(&dir->obj, &dir->body))
		return -EINVAL;

	/* Does the found item plugin match  */
	if (dir->body.plug != bplug)
		return RE_FATAL;

	/* Does the body item belong to the current object. */
	return plug_call(dir->body.key.plug->o.key_ops, compshort,
			 &dir->body.key, &dir->offset) ? RE_FATAL : 0;
}

errno_t dir40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   region_func_t region_func,
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
	
	if ((res = obj40_stat_launch(&dir->obj, NULL, 
				     dir40_exts, 1, S_IFDIR, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(object, &info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_fix_key(&dir->obj, &info->start, &info->object, mode)) < 0)
		return res;
	
	/* Init hash plugin in use. */
	dir->hash = obj40_plug_recognize(&dir->obj, HASH_PLUG_TYPE, "hash");
	
	if (dir->hash == NULL) {
                aal_exception_error("Directory %s: failed to init hash plugin."
				    "Plugin (%s).", print_ino(dcore, &info->object),
				    dir40_plug.label);
                return -EINVAL;
        }
	
	/* FIXME-VITALY: take it from SD first. But of which type -- there is 
	   only ITEM_TYPE for now. */
	if ((pid = dcore->param_ops.value("direntry")) == INVAL_PID) {
		aal_exception_error("Failed to get a plugid for direntry from "
				    "the params.");
		return -EINVAL;
	}
	
	if ((bplug = dcore->factory_ops.ifind(ITEM_PLUG_TYPE, pid)) == NULL) {
		aal_exception_error("Failed to find direntry plugin by "
				    "the id %d.", pid);
                 return -EINVAL;
	}

	/* Take case about the ".". */
	if ((res |= dir40_dot(dir, bplug, mode)) < 0)
		return res;
	
	size = 0; bytes = 0; 
	
	/* FIXME-VITALY: this probably should be changed. Now hash plug that is
	   used is taken from SD or the default one from the params. Probably it
	   would be better to do evth in vise versa order -- choose the hash
	   found among the entries most of the times and correct hash plugin in
	   SD. */
	while (TRUE) {
		pos_t *pos = &dir->body.pos;
		trans_hint_t hint;
		key_entity_t key;
		uint32_t units;
		errno_t ret;
		
		/* Check that the body item is of the current dir. */
		if ((ret = dir40_belongs(dir, bplug)) < 0)  {
			return ret;
		} else if (ret) {
			/* Not of the current dir. */
			break;
		}
		
		if (dir->body.plug->id.group != DIRENTRY_ITEM) {
			/* FIXME-VITALY: break; for now -- delete the item
			   as it matches by keys. -- this all is handled in 
			   dir40_belongs -- fix it also. */
			break;
		}
		    
		/* Try to register the item if it has not been yet. Any 
		   item has a pointer to objectid in the key, if it is 
		   shared between 2 objects, it should be already solved 
		   at relocation time. */
		if (place_func && place_func(object, &dir->body, data))
			return -EINVAL;
		
		units = plug_call(dir->body.plug->o.item_ops, 
				  units, &dir->body);
		
		for (pos->unit = 0; pos->unit < units; pos->unit++) {
			/*  */
			if ((res |= dir40_fetch(object, &entry)) < 0)
				return res;
			
			plug_call(entry.offset.plug->o.key_ops, build_entry, 
				  &key, dir->hash, obj40_locality(&dir->obj),
				  obj40_objectid(&dir->obj), entry.name);
	
			if (!plug_call(key.plug->o.key_ops, compfull, 
				       &key, &entry.offset))
				continue;
			
			/* Broken entry found, remove it. */
			aal_exception_error("Directory [%s], plugin %s, node "
					    "[%llu], item [%u], unit [%u]: "
					    "entry has wrong offset [%s]."
					    " Should be [%s]. %s", 
					    print_ino(dcore, &info->object),
					    dir40_plug.label, 
					    dir->body.block->nr,
					    dir->body.pos.item, 
					    dir->body.pos.unit,
					    print_key(dcore, &entry.offset),
					    print_key(dcore, &key), 
					    mode == RM_BUILD ? "Removed." : "");


			if (mode != RM_BUILD) {
				res |= RE_FIXABLE;
				continue;
			}

			hint.count = 1;

			/* FIXME-VITALY: make sure that the tree does not 
			   get rebalanced while removing the entry. Also 
			   I suppose that removing the last unit will remove 
			   the whole item. */
			if ((res |= obj40_remove(&dir->obj, &dir->body, 
						 &hint)) < 0)
				return res;

			units--; 
			pos->unit--;
		}
		
		if (units) {
			/* Count size and bytes. */
			size += plug_call(dir->body.plug->o.item_ops, 
					  size, &dir->body);

			bytes += plug_call(dir->body.plug->o.item_ops, 
					   bytes, &dir->body);
			
			if ((res |= dcore->tree_ops.next(dir->obj.info.tree,
							&dir->body, 
							&dir->body)) < 0)
				return res;

			if (!dir->body.node)
				break;
		} else {
			
			/* Lookup the last removed entry, get the next item. */
			if (dir40_lookup(object, entry.name, &entry) < 0)
				return -EIO;

			dir->body = entry.place;
		}
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

errno_t dir40_check_attach(object_entity_t *object, object_entity_t *parent, 
			   uint8_t mode)
{
	dir40_t *dir = (dir40_t *)object;
	lookup_t lookup;
	entry_hint_t entry;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	lookup = dir40_lookup(object, "..", &entry);

	switch(lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!plug_call(entry.object.plug->o.key_ops, compfull, 
			       &entry.object, &parent->info.object))
			break;
		
		/* Already attached. */
		aal_exception_error("Directory [%s], plugin %s: the object "
				    "is attached already to [%s] and cannot "
				    "be attached to [%s].", 
				    print_ino(dcore, &object->info.object),
				    dir40_plug.label, 
				    print_key(dcore, &entry.object),
				    print_ino(dcore, &parent->info.object));

		return RE_FATAL;
	case ABSENT:
		/* Not attached yet. */
		aal_exception_error("Directory [%s], plugin %s: the object "
				    "is not attached. %s [%s].", 
				    print_ino(dcore, &object->info.object),
				    dir40_plug.label, mode == RM_CHECK ? 
				    "Reached from" : "Attaching to",
				    print_ino(dcore, &parent->info.object));
	
		if (mode == RM_CHECK)
			return RE_FIXABLE;
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&dir->obj)->plug->o.key_ops, assign,
			  &entry.object, &parent->info.object);
		
		aal_strncpy(entry.name, "..", sizeof(entry.name));
		
		if ((res = plug_call(object->plug->o.object_ops,
				     add_entry, object, &entry)))
			return res;

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
	obj40_init(&dir->obj, &dir40_plug, dcore, info);
	
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

	/* Init hash plugin in use if is not known yet. */
	if (!dir->hash) {
		dir->hash = obj40_plug_recognize(&dir->obj, HASH_PLUG_TYPE, 
						 "hash");

		if (dir->hash == NULL) {
			aal_exception_error("Directory %s: failed to init "
					    "hash plugin. Plugin (%s).", 
					    print_ino(dcore, &dir->obj.info.object),
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
