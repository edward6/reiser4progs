/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    librepair/object.c - Object consystency recovery code. */

#include <repair/object.h>
#include <repair/item.h>


/* Check the semantic structure of the object. Mark all items as CHECKED. */
errno_t repair_object_check_struct(reiser4_object_t *object, 
				   place_func_t place_func, 
				   uint8_t mode, void *data) 
{
	errno_t res;
	
	aal_assert("vpf-1044", object != NULL);
	
	if ((res = plugin_call(object->entity->plugin->o.object_ops, 
			       check_struct, object->entity, &object->info,
			       place_func, mode, data)))
		return res;
	
	/* FIXME-VITALY: this is probably should be set by plugin. Together 
	   with object->info.parent key. */
	reiser4_key_assign(&object->info.object, &object->info.start.item.key);
	reiser4_key_string(&object->info.object, object->name);
	
	return 0;
}

/* Helper callback for probing passed @plugin. 
   
   FIXME-VITALY: for now it returns the first matched plugin, it should 
   be changed if plugins are not sorted in some order of adventages of 
   recovery. */
static bool_t callback_object_realize(reiser4_plugin_t *plugin, void *data) {
	reiser4_object_t *object;
	
	/* We are interested only in object plugins here */
	if (plugin->id.type != OBJECT_PLUGIN_TYPE)
		return FALSE;
	
	object = (reiser4_object_t *)data;
	
	/* Try to realize the object as an instance of this plugin. */
	object->entity = plugin_call(plugin->o.object_ops, realize, 
				     &object->info);
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
		
		/* If the pointed item was found, object must be opanable. 
		   @parent probably should be passed here. */
		object = reiser4_object_realize(tree, &place);
		
		if (!object)
			return NULL;
		
		break;
	case ABSENT:
		if (!(object = aal_calloc(sizeof(*object), 0)))
			return NULL;
		
		object->info.tree = tree;
		object->info.object = *key;
		
		/*
		if (parent)
			object->info.parent = parent->info.object;
		*/
		
		libreiser4_factory_cfind(callback_object_realize, 
					 object, FALSE);
		
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
					reiser4_place_t *place,
					bool_t only) 
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
	
	libreiser4_factory_cfind(callback_object_realize, object, only);
	
	if (!object->entity)
		goto error_close_object;
	
	reiser4_key_string(&object->info.object, object->name);
	
	return object;
	
 error_close_object:
	aal_free(object);
	return NULL;
}

errno_t repair_object_traverse(reiser4_object_t *object,
			       object_open_func_t open_func,
			       object_close_func_t close_func,
			       void *data)
{
	entry_hint_t entry;
	errno_t res = 0;
	
	aal_assert("vpf-1090", object != NULL);
	aal_assert("vpf-1092", object->info.tree != NULL);
	aal_assert("vpf-1103", open_func != NULL);
	aal_assert("vpf-1166", close_func != NULL);
	
	while (!reiser4_object_readdir(object, &entry)) {
		reiser4_object_t *child = NULL;
		
		/* Some entry was read. Try to detect the object of 
		   the paticular plugin pointed by this entry. */
		if ((child = open_func(object, &entry, data)) == INVAL_PTR)
			return -EINVAL;
		
		if (child == NULL)
			continue;
		
		res = repair_object_traverse(child, open_func, 
					     close_func, data);
		
		close_func(child, data);
		
		if (res)
			return res;
	}
	
	return 0;
}

/* @parent is the object where the @object name of the type @type was found.
   Check the backlink -- '..' for directories if @type == NAME, that name 
   exists if @type == DOTDOT, etc. On REBUILD pass, insert the name if missed
   and fix '..' to point correctly. If the object allow only one NAME and was 
   reached already -- FATAL corruption is returned. */
errno_t repair_object_check_backlink(reiser4_object_t *object, 
				     reiser4_object_t *parent, 
				     entry_type_t type,
				     uint8_t mode)
{
	aal_assert("vpf-1044", object != NULL);
	aal_assert("vpf-1098", object->entity != NULL);
	aal_assert("vpf-1099", parent != NULL);
	aal_assert("vpf-1100", parent->entity != NULL);
	
	if (!object->entity->plugin->o.object_ops->check_backlink)
		return 0;
	
	return object->entity->plugin->o.object_ops->check_backlink(object->entity,
								    parent->entity,
								    type, mode);
}

