/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "dir40.h"
#include "repair/plugin.h"

extern reiser4_plugin_t dir40_plugin;
extern lookup_t dir40_lookup(object_entity_t *entity, char *name, 
			     entry_hint_t *entry);

static errno_t callback_mode_dir(uint16_t mode) {
	return S_ISDIR(mode) ? 0 : -EINVAL;
}

static errno_t callback_type_name(uint16_t type) {
	return type == KEY_FILENAME_TYPE ? 0 : -EINVAL;
}

object_entity_t *dir40_realize(object_info_t *info) {
	dir40_t *dir;
	
	if (obj40_realize(info, callback_mode_dir, callback_type_name))
		return NULL;

	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return NULL;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plugin, NULL, core, info->tree);
	
	return (object_entity_t *)dir;
}

errno_t dir40_check_backlink(object_entity_t *object, object_entity_t *parent, 
			     entry_type_t type, uint8_t mode)
{
	entry_hint_t entry;
	lookup_t lookup;
	uint32_t links;
	dir40_t *dir;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	dir = (dir40_t *)object;
	
	switch(type) {
	case ET_NAME:
		/* @object was reached by NAME from @parent. Recover '..'. */
		aal_strncpy(entry.name, "..", 2);
		
		lookup = dir40_lookup(object, entry.name, &entry);
		
		switch(lookup) {
		case PRESENT:
			/* If the key matches the parent -- ok. */
/*			if (!plugin_call(entry.object.plugin->o.key_ops, 
					 compare, &entry.object, 
					 STAT_KEY(&parent->obj)))
				return REPAIR_OK;*/
			
			/* Different keys can be fixed in REBUILD mode only. */
			if (mode != REPAIR_REBUILD)
				return REPAIR_FATAL;
			
			links = obj40_get_nlink(&dir->obj);
			
			/* If the dir has been reached already from somewhere 
			   and '..' was left, that means that '..' is valid.
			   As dir40 allows only one name and this is another
			   one, this is an error. */
			if (links > 1)
				return REPAIR_FATAL;

			/* Set the correct object key. */
			
		case ABSENT:
			/* Adding ".." to object. */
/*			plugin_call(STAT_KEY(&dir->obj)->plugin->o.key_ops,
				    assign, &entry.object, STAT_KEY(&parent->obj));*/
			
			if ((res = plugin_call(object->plugin->o.object_ops,
					       add_entry, object, &entry)))
			{
				return res;
			}
		case FAILED:
			return -EINVAL;
		}
		
		return REPAIR_OK;
	case ET_PARENT:
		/* Find the pointer to the @parent in the @object. */
		/* @object was reached by '..' from @parent. Recover its name. */
	
		aal_assert("vpf-1154", parent->plugin->o.object_ops->lookup != NULL);
		parent->plugin->o.object_ops->lookup(parent, entry.name, &entry);
	default:
		break;
	}
		
	return REPAIR_OK;
}

#endif

