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
extern lookup_t dir40_lookup(object_entity_t *entity, char *name, 
			     entry_hint_t *entry);

static errno_t callback_mode(uint16_t mode) {
	return S_ISDIR(mode) ? 0 : -EINVAL;
}

static errno_t callback_type(uint16_t type) {
	return type == KEY_FILENAME_TYPE ? 0 : -EINVAL;
}

/* Build the key of the '.'. */
static errno_t callback_body(object_info_t *info, key_entity_t *key) {
	uint64_t locality, objectid;
	
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
		
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);
	
	plug_call(info->object.plug->o.key_ops, build_entry, 
		  key, NULL, locality, objectid, ".");

	return 0;
}

object_entity_t *dir40_realize(object_info_t *info) {
	dir40_t *dir;
	errno_t res;
	
	if ((res = obj40_realize(info, callback_mode, callback_type, 
				 callback_body)))
		return res < 0 ? INVAL_PTR : NULL;
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plug, NULL, core, info->tree);
	
	return (object_entity_t *)dir;
}

errno_t dir40_check_attach(object_entity_t *object, object_entity_t *parent, 
			   uint8_t mode)
{
	dir40_t *odir = (dir40_t *)object;
	dir40_t *pdir = (dir40_t *)parent;
	entry_hint_t entry;
	lookup_t lookup;
	uint32_t links;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	aal_strncpy(entry.name, "..", 2);

	lookup = dir40_lookup(object, entry.name, &entry);

	switch(lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!plug_call(entry.object.plug->o.key_ops, compfull, 
			       &entry.object, STAT_KEY(&pdir->obj)))
			break;

		return REPAIR_FATAL;
	case ABSENT:
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&odir->obj)->plug->o.key_ops, assign,
			  &entry.object, STAT_KEY(&pdir->obj));
		
		if ((res = plug_call(object->plug->o.object_ops,
				     add_entry, object, &entry)))
			return res;
	case FAILED:
		return -EINVAL;
	}

	/* ".." matches the parent. Now do parent->nlink++ for REBUILD mode. */
	return mode == REPAIR_REBUILD ? 
	       plug_call(parent->plug->o.object_ops, link, parent) :
	       REPAIR_OK;
}

#endif

