/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "dir40.h"
#include "repair/plugin.h"

extern reiser4_plug_t dir40_plug;

extern errno_t dir40_reset(object_entity_t *entity);
extern lookup_t dir40_lookup(object_entity_t *entity, char *name, 
			     entry_hint_t *entry);

#define known_extentions ((uint64_t)1 << SDEXT_UNIX_ID | 	\
			  	    1 << SDEXT_LW_ID |		\
				    1 << SDEXT_PLUG_ID)

static errno_t dir40_extentions(place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	extmask &= ~known_extentions;

	return extmask ? RE_FATAL : RE_OK;
}

/* Check SD extentions and that mode in LW extention is DIRFILE. */
static errno_t callback_stat(place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = dir40_extentions(stat)))
		return res;

	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	return S_ISDIR(lw_hint.mode) ? 0 : RE_FATAL;
}

/* Set the key of "." taken from @info->start into @info->object */
static errno_t callback_key(obj40_t *obj) {
	entry_hint_t entry;
	
	if (obj->info.start.plug->o.item_ops->read == NULL)
		return -EINVAL;
	
	/* Read the first entry. */
	if (plug_call(obj->info.start.plug->o.item_ops, read, 
		      &obj->info.start, &entry, 0, 1) != 1)
		return -EINVAL;
	
	/* If not "." -- cannot obtain the "." key. */
	if (aal_strlen(entry.name) != 1 || aal_strncmp(entry.name, ".", 1))
		return RE_FATAL;
	
	aal_memcpy(&obj->info.object, &entry.object, sizeof(entry.object));
	
	return 0;
}

object_entity_t *dir40_realize(object_info_t *info) {
	dir40_t *dir;
	errno_t res;
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plug, core, info);
	
	if ((res = obj40_realize(&dir->obj, callback_stat, callback_key)))
		goto error;
	
	/* Positioning to the first directory unit */
	if (dir40_reset((object_entity_t *)dir))
		goto error;
	
	return (object_entity_t *)dir;
 error:
	aal_free(dir);
	return res < 0 ? INVAL_PTR : NULL;
}

static void dir40_one_nlink(uint32_t *nlink) {
	*nlink = 1;
}

static void dir40_check_mode(uint16_t *mode) {
	if (!S_ISDIR(*mode)) {
		*mode &= ~S_IFMT;
        	*mode |= S_IFDIR;
	}
}

static void dir40_check_size(uint64_t *sd_size, uint64_t counted_size) {
	/* FIXME-VITALY: This is not correct for extents as the last 
	   block can be not used completely. Where to take the policy
	   plugin to figure out if size is correct? */
	if (*sd_size != counted_size)
		*sd_size = counted_size;
}

errno_t dir40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   region_func_t region_func,
			   uint8_t mode, void *data)
{
	uint64_t locality, objectid, ordering;
	uint64_t size, bytes, offset, next;
	dir40_t *dir = (dir40_t *)object;
	reiser4_plug_t *bplug;
	object_info_t *info;
	errno_t res = RE_OK;
	key_entity_t key;
	lookup_t lookup;
	
	aal_assert("vpf-1224", dir != NULL);
	aal_assert("vpf-1190", dir->obj.info.tree != NULL);
	aal_assert("vpf-1197", dir->obj.info.object.plug != NULL);
	
	info = &dir->obj.info;
	
	if ((res = obj40_stat_launch(&dir->obj, dir40_extentions, 
				     1, S_IFDIR, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(object, &info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res |= obj40_ukey(&dir->obj, &info->start, &info->object, mode)))
		return res;
	
	/* Init hash plugin in use. */
	if (!(dir->hash = obj40_plug(&info->start, HASH_PLUG_TYPE, "hash"))) {
                aal_exception_error("Can't init hash plugin for directory %s. "
				    "Plugin (%s).", print_ino(core, &info->object),
				    dir40_plug.label);
                return -EINVAL;
        }

	/* Init body plugin in use. */
	if (!(bplug = obj40_plug(&info->start, ITEM_PLUG_TYPE, "direntry"))) {
                aal_exception_error("Can't init hash plugin for directory %s. "
				    "Plugin (%s).", print_ino(core, &info->object),
				    dir40_plug.label);
                return -EINVAL;
        }
	
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
	
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);

	ordering = plug_call(info->object.plug->o.key_ops,
			     get_ordering, &info->object);

	/* Build the start key of the body. */
	plug_call(info->object.plug->o.key_ops, build_entry,
		  &key, dir->hash, locality, objectid, ".");

	size = 0; bytes = 0; 
	
	/* FIXME-VITALY: this probably should be changed. Now hash plug
	   that is used is default, passed here from outside, or taken 
	   from SD. Probably it would be better to do evth in vise versa 
	   order -- choose the hash found among the entries most of the 
	   times and correct hash plugin in SD. */
	while (TRUE) {
		if ((lookup = obj40_lookup(&dir->obj, &key, LEAF_LEVEL, 
					   &dir->body)) == FAILED)
			return -EINVAL;
		
		if (lookup == ABSENT) {
			/* If place is invalid, no more items. */
			if (!core->tree_ops.valid(info->tree, &dir->body))
				break;
			
			/* Initializing item entity at @next place */
			if ((res |= core->tree_ops.fetch(info->tree, &dir->body)))
				return res;
			
			/* Check if this is an item of another object. */
			if (plug_call(key.plug->o.key_ops, compshort, 
				      &key, &dir->body.key))
				break;
		}
		
		/* Does the found item plugin match  */
		if (dir->body.plug != bplug)
			break;

		
	}
	
	/* Take care about "." */
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL))
		res |= obj40_check_stat(&dir->obj, mode == RM_BUILD ?
					dir40_one_nlink : NULL,
					dir40_check_mode,
					dir40_check_size, 
					size, bytes, mode);
	
	return res;
}

errno_t dir40_check_attach(object_entity_t *object, object_entity_t *parent, 
			   uint8_t mode)
{
	dir40_t *odir = (dir40_t *)object;
	key_entity_t *par_key;
	entry_hint_t entry;
	lookup_t lookup;
	uint32_t links;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	aal_strncpy(entry.name, "..", 2);

	lookup = dir40_lookup(object, entry.name, &entry);

	if (!(par_key = plug_call(parent->plug->o.object_ops, origin,  parent)))
		return -EINVAL;
	
	switch(lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!plug_call(entry.object.plug->o.key_ops, compfull, 
			       &entry.object, par_key))
			break;

		return RE_FATAL;
	case ABSENT:
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&odir->obj)->plug->o.key_ops, assign,
			  &entry.object, par_key);
		
		if ((res = plug_call(object->plug->o.object_ops,
				     add_entry, object, &entry)))
			return res;
	case FAILED:
		return -EINVAL;
	}

	/* ".." matches the parent. Now do parent->nlink++ for REBUILD mode. */
	return mode != RM_BUILD ? RE_OK :
	       plug_call(parent->plug->o.object_ops, link, parent);
}

#endif
