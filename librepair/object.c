/*  Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
    
    librepair/object.c - Object consystency recovery code. */

#include <repair/object.h>
#include <repair/item.h>

/* Callback for repair_object_check_struct. Mark the passed item as CHECKED. */
errno_t callback_check_struct(object_entity_t *object, place_t *place, 
			      void *data) 
{
	aal_assert("vpf-1114", object != NULL);
	aal_assert("vpf-1115", place != NULL);
	
	repair_item_set_flag((reiser4_place_t *)place, ITEM_CHECKED);
	
	return 0;
}

/* Check the semantic structure of the object. Mark all items as CHECKED. */
errno_t repair_object_check_struct(reiser4_object_t *object, 
				   place_func_t place_func, 
				   uint8_t mode, void *data) 
{
	errno_t res;
	
	aal_assert("vpf-1044", object != NULL);
	
	res = plugin_call(object->entity->plugin->o.object_ops, check_struct,
			  object->entity, &object->info, place_func, mode, data);
	
	if (res)
		return -EINVAL;
	
	/* FIXME-VITALY: this is probably should be set by plugin. Together with 
	   object->info.parent key. */
	reiser4_key_assign(&object->info.object, &object->info.start.item.key);
	reiser4_key_string(&object->info.object, object->name);
	
	return 0;
}

#if 0
errno_t repair_object_launch(reiser4_object_t *object) {
	aal_assert("vpf-1097", object != NULL);
	
	reiser4_key_assign(&object->info.object, &object->info.start.item.key);
	reiser4_key_string(&object->info.object, object->name);
	
	return reiser4_object_guess(object);
}

inline void repair_object_init(reiser4_object_t *object, 
			       reiser4_tree_t *tree, reiser4_place_t *place, 
			       reiser4_key_t *parent, reiser4_key_t *key)
{
	aal_memset(object, 0, sizeof(*object));
	object->info.tree = tree;
	
	if (place)
		aal_memcpy(reiser4_object_start(object), place, sizeof(*place));
	
	if (parent)
		aal_memcpy(&object->info.parent, parent, sizeof(*parent));
	
	if (key)
		aal_memcpy(&object->info.object, key, sizeof(*key));
}
#endif

/* Helper callback for probing passed @plugin. 
   
   FIXME-VITALY: for now it returns the first matched plugin, it should be 
   changed if plugins are not sorted in some order of adventages of recovery. */
static bool_t callback_object_open(reiser4_plugin_t *plugin, void *data) {
	reiser4_object_t *object;
	
	/* We are interested only in object plugins here */
	if (plugin->h.type != OBJECT_PLUGIN_TYPE)
		return FALSE;
	
	object = (reiser4_object_t *)data;
	
	/* Try to realize the object as an instance of this plugin. */
	object->entity = plugin_call(plugin->o.object_ops, realize, &object->info);
	return object->entity ? TRUE : FALSE;
}

/* Open the object on the base of given start @key */
reiser4_object_t *repair_object_launch(reiser4_tree_t *tree, 
				       reiser4_key_t *key)
{
	reiser4_object_t *object;
	reiser4_place_t place;
	lookup_t lookup;
	
	aal_assert("vpf-1132", tree != NULL);
	aal_assert("vpf-1134", key != NULL);
	
	lookup = reiser4_tree_lookup(tree, key, LEAF_LEVEL, &place);
	
	switch(lookup) {
	case PRESENT:
		/* The start of the object seems to be found. */
		if (reiser4_place_realize(&place))
			return NULL;

		/* The key must point to the start of the object. */
		if (reiser4_key_compare(&place.item.key, key))
			return NULL;
		
		object = reiser4_object_realize(tree, &place);
		
		if (!object)
			return NULL;
		
		break;
	case ABSENT:
		if (!(object = aal_calloc(sizeof(*object), 0)))
			return NULL;
		
		object->info.tree = tree;
		object->info.object = *key;
		
		libreiser4_factory_cfind(callback_object_open, object, FALSE);
		
		if (!object->entity)
			goto error_close_object;

		reiser4_key_string(&object->info.object, object->name);

		break;
	case FAILED:
		return NULL;
	}
	
	return object;

 error_close_object:
	aal_free(object);
	return NULL;
}

/* Open the object by some place - not nessesary the first one. */
reiser4_object_t *repair_object_realize(reiser4_tree_t *tree, 
					reiser4_place_t *place) 
{
	reiser4_object_t *object;
	
	aal_assert("vpf-1131", tree != NULL);
	aal_assert("vpf-1130", place != NULL);
	aal_assert("vpf-1130", place->item.plugin != NULL);
	
	if (reiser4_item_statdata(place))
		return reiser4_object_realize(tree, place);
	
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;
    	
	object->info.tree = tree;
	
	aal_memcpy(reiser4_object_start(object),
		   place, sizeof(*place));
	
	reiser4_key_assign(&object->info.object,
			   &object->info.start.item.key);
	
	libreiser4_factory_cfind(callback_object_open, object, FALSE);
	
	if (!object->entity)
		goto error_close_object;
	
	reiser4_key_string(&object->info.object, object->name);
	
	return object;
	
 error_close_object:
	aal_free(object);
	return NULL;
}

#if 0
/* Try to recognized the object plugin by the given @object->info. Either 
   object->info.place must be valid or object->info.object and object->info.parent 
   keys. */
reiser4_plugin_t *repair_object_realize(reiser4_object_t *object) {
	reiser4_plugin_t *plugin;
	lookup_t lookup;
	rid_t pid;
	
	aal_assert("vpf-1083", object != NULL); 
	aal_assert("vpf-1084", object->info.tree != NULL); 
	
	do {
		/* Realize by specified key, looking up its start place. */
		if (object->info.object.plugin != NULL) {
			lookup = reiser4_tree_lookup(object->info.tree, 
						     &object->info.object, 
						     LEAF_LEVEL, 
						     reiser4_object_start(object));
			
			if (lookup == FAILED)
				return NULL;
			
			if (lookup != PRESENT)
				break;
			
			/* The start of the object seems to be found, is it SD? */
			if (reiser4_place_realize((reiser4_object_start(object))))
				return NULL;

			/* The key must point to the start of the object. */
			if (reiser4_key_compare(&object->info.object,
						&object->start.item.key))
				return NULL;
		}
		
		/* If it is stat data, try to get object plugin from it. */
		if (!reiser4_item_statdata((reiser4_object_start(object))))
			break;
		
		plugin = object->info.start.item.plugin;
		
		/* This is an SD found, try to get object plugin id from it. */
		if (!plugin->o.item_ops->get_plugid)
			break;
		
		pid = plugin->o.item_ops->get_plugid(&object->info.start.item, 
						     OBJECT_PLUGIN_TYPE);
		
		if (pid == INVAL_PID)
			break;
		
		/* Plugin id was obtained from the SD. Get the plugin. */
		if (!(plugin = libreiser4_factory_ifind(OBJECT_PLUGIN_TYPE, pid)))
			break;
		
		/* Try to realize the object with this plugin. */
		if (!plugin_call(plugin->o.object_ops, realize, &object->info))
			return plugin;
	} while (FALSE);
	
	/* Try all plugins to realize the object, choose the more preferable.
	   FIXME-VITALY: plugins are not sorted yet in the list. */
	return libreiser4_factory_cfind(callback_object_guess, &object->info, FALSE);
}

#endif

errno_t repair_object_traverse(reiser4_object_t *object, traverse_func_t func, 
			       void *data) 
{
	entry_hint_t entry;
	errno_t res = 0;
	
	aal_assert("vpf-1090", object != NULL);
	aal_assert("vpf-1092", object->info.tree != NULL);
	aal_assert("vpf-1103", func != NULL);
	
	while (!reiser4_object_readdir(object, &entry)) {
		reiser4_object_t *child = NULL;
		
		/* Some entry was read. Try to detect the object of the paticular 
		   plugin pointed by this entry. */	
		if ((res = func(object, &child, &entry, data)))
			return res;
		
		if (child == NULL)
			continue;
		
		if ((res = repair_object_traverse(child, func, data)))
			return res;
		
		reiser4_object_close(child); 
	}
	
	return 0;
}

/* Check '..' entry of directories. Fix the parent pointer if needed, mark REACHABLE 
   if the parent pointer in the object matches the parent pointer, nlink++. */
errno_t repair_object_check_link(reiser4_object_t *object, 
				 reiser4_object_t *parent, 
				 uint8_t mode) 
{
	aal_assert("vpf-1044", object != NULL);
	aal_assert("vpf-1098", object->entity != NULL);
	aal_assert("vpf-1099", parent != NULL);
	aal_assert("vpf-1100", parent->entity != NULL);
	
	repair_item_set_flag(reiser4_object_start(object), ITEM_REACHABLE);
	
	return plugin_call(object->entity->plugin->o.object_ops, check_link, 
			   object->entity, parent->entity, mode);
}

