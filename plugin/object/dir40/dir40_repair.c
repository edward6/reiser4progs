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

#define dir40_zero_extentions(sd, mask)					\
({									\
	if ((mask = obj40_extmask(sd)) == MAX_UINT64)			\
		return -EINVAL;						\
									\
	mask &= ~((uint64_t)(1 << SDEXT_UNIX_ID) | (1 << SDEXT_LW_ID) | \
		  (1 << SDEXT_PLUG_ID));				\
})

static errno_t dir40_extentions(place_t *sd) {
	uint64_t extmask;
	
	dir40_zero_extentions(sd, extmask);
	
	return extmask ? RE_FATAL : RE_OK;
}

/* Check SD extentions and that mode in LW extention is DIRFILE. */
static errno_t callback_stat(place_t *sd) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = dir40_extentions(sd)))
		return res;

	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(sd, SDEXT_LW_ID, &lw_hint)) < 0)
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

/* Fix place key if differs from @key. */
static errno_t dir40_ukey(dir40_t *dir, place_t *place, key_entity_t *key, 
			  uint8_t mode) 
{
	object_info_t *info;
	errno_t res;
	
	aal_assert("vpf-1218", dir != NULL);
	
	info = &dir->obj.info;
	
	if (!key->plug->o.key_ops->compfull(key, &place->key))
		return 0;
	
	aal_exception_error("Node (%llu), item(%u): the key [%s] of the "
			    "item is wrong, %s [%s]. Plugin (%s).", 
			    place->block->nr, place->pos.unit, 
			    core->key_ops.print(&place->key, PO_DEF),
			    mode == RM_BUILD ? "fixed to" : "should be", 
			    core->key_ops.print(key, PO_DEF), 
			    dir->obj.plug->label);
	
	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((res = core->tree_ops.ukey(info->tree, place, key))) {
		aal_exception_error("Node (%llu), item(%u): update of the "
				    "item key failed.", place->block->nr,
				    place->pos.unit);
	}

	return res;
}

/* SD not found, create a new one. This is a special case and is not used 
   usually (only for "/" and "lost+found" recovery), but just skipped as 
   cannot be realized by any plugin. */
static errno_t dir40_recreate_stat(dir40_t *dir, uint8_t mode) {
	key_entity_t *key;
	uint64_t pid;
	errno_t res;
	
	key = &dir->obj.info.object;
	
	aal_exception_error("Dirfile [%s] does not have a StatData item.%s"
			    "Plugin %s.", core->key_ops.print(key, PO_INO),
			    mode == RM_BUILD ? " Creating a new one." : "",
			    dir->obj.plug->label);
	
	if (mode != RM_BUILD)
		return RE_FATAL;
	
	pid = core->profile_ops.value("statdata");
	
	if (pid == INVAL_PID)
		return -EINVAL;
	
	if ((res = obj40_create_stat(&dir->obj, pid, 0, 0,  1, S_IFDIR))) {
		aal_exception_error("Dirfile [%s] failed to create "
				    "a StatData item. Plugin %s.",
				    core->key_ops.print(key, PO_INO),
				    dir->obj.plug->label);
	}
	
	return res;
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
	uint64_t size, bytes, offset, next;
	dir40_t *dir = (dir40_t *)object;
	object_info_t *info;
	errno_t res = RE_OK;
	lookup_t lookup;
	
	aal_assert("vpf-1224", dir != NULL);
	aal_assert("vpf-1190", dir->obj.info.tree != NULL);
	aal_assert("vpf-1197", dir->obj.info.object.plug != NULL);
	
	info = &dir->obj.info;
	
	/* Update the place of SD. */
	lookup = core->tree_ops.lookup(info->tree, &info->object,
				       LEAF_LEVEL, &info->start);
	
	if (lookup == FAILED)
		return -EINVAL;
	
	if (lookup == ABSENT) {
		/* If SD is not correct. Create a new one. */
		if ((res = obj40_stat(&dir->obj, dir40_extentions)) < 0)
			return res;
		
		if (res && (res = dir40_recreate_stat(dir, mode)))
			return res;
	} else {
		/* If SD is not correct. Fix it if needed. */
		uint64_t extmask;
		
		dir40_zero_extentions(&info->start, extmask);
		
		if (extmask) {
			aal_exception_error("Node (%llu), item (%u): statdata "
					    "has unknown set of extentions "
					    "(0x%llx). Plugin (%s)", 
					    info->start.block->nr, 
					    info->start.pos.item, extmask,
					    info->start.plug->label);
			return RE_FATAL;
		}
	}
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(object, &info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res |= dir40_ukey(dir, &info->start, &info->object, mode)))
		return res;
	
	while (TRUE) {
	}

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

