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
	
	if ((res = plug_call(object->entity->plug->o.object_ops, 
			     check_struct, object->entity, &object->info,
			     place_func, mode, data)) < 0)
	
	repair_error_check(res, mode);
	aal_assert("vpf-1195", mode != REPAIR_REBUILD ||
			      !(res & REPAIR_FATAL));
	
	reiser4_key_assign(&object->info.object, &object->info.start.key);
	reiser4_key_string(&object->info.object, object->name);
	
	return res;
}

/* Helper callback for probing passed @plugin. */
static bool_t callback_object_realize(reiser4_plug_t *plug, void *data) {
	reiser4_object_t *object;
	
	/* We are interested only in object plugins here */
	if (plug->id.type != OBJECT_PLUG_TYPE)
		return FALSE;
	
	object = (reiser4_object_t *)data;
	
	/* Try to realize the object as an instance of this plugin. */
	object->entity = plug_call(plug->o.object_ops, realize, 
				   &object->info);
	
	if (object->entity != NULL && object->entity != INVAL_PTR) {
		plug_call(plug->o.object_ops, close,  object->entity);
		return TRUE;
	}
	
	return FALSE;
}

/* Open the object on the base of given start @key */
reiser4_object_t *repair_object_launch(reiser4_tree_t *tree,
				       reiser4_key_t *key, 
				       bool_t only)
{
	reiser4_object_t *object;
	reiser4_place_t place;
	lookup_t lookup;
	
	aal_assert("vpf-1132", tree != NULL);
	aal_assert("vpf-1134", key != NULL);
	
	lookup = reiser4_tree_lookup(tree, key, LEAF_LEVEL, &place);
	
	switch(lookup) {
	case PRESENT:
		/* The start of the object seems to be found. The key must point 
		   to the start of the object. */
		if (reiser4_key_compare(&place.key, key))
			return NULL;
		
		/* If the pointed item was found, object must be opanable. 
		   @parent probably should be passed here. */
		object = reiser4_object_realize(tree, &place);
		
		if (!object)
			return NULL;
		
		break;
	case ABSENT:
		if (!(object = aal_calloc(sizeof(*object), 0)))
			return INVAL_PTR;
		
		object->info.tree = tree;
		object->info.object = *key;
		
		/*
		if (parent)
			object->info.parent = parent->info.object;
		*/
		
		libreiser4_factory_cfind(callback_object_realize, 
					 object, only);
		
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
	aal_assert("vpf-1189", place->plug != NULL);
	
	/* If StatData found -- it handles some object, try to realize it. */
	if (reiser4_item_statdata(place))
		return reiser4_object_realize(tree, place);
	
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;
    	
	object->info.tree = tree;
	
	aal_memcpy(reiser4_object_start(object), place, sizeof(*place));
	
	libreiser4_factory_cfind(callback_object_realize, object, only);
	
	if (!object->entity)
		goto error_close_object;
	
	reiser4_key_assign(&object->info.object, &object->info.start.key);
	reiser4_key_string(&object->info.object, object->name);
	
	return object;
	
 error_close_object:
	aal_free(object);
	return NULL;
}

/* Checks the attach between @parent and @object */
errno_t repair_object_check_attach(reiser4_object_t *parent, 
				   reiser4_object_t *object, 
				   uint8_t mode)
{
	reiser4_plug_t *plug;
	errno_t res;
	
	aal_assert("vpf-1188", object != NULL);
	aal_assert("vpf-1098", object->entity != NULL);
	aal_assert("vpf-1099", parent != NULL);
	aal_assert("vpf-1100", parent->entity != NULL);
	
	plug = object->entity->plug;
	
	if (!object->entity->plug->o.object_ops->check_attach)
		return 0;
	
	if ((res = plug_call(object->entity->plug->o.object_ops, check_attach,
			     object->entity, parent->entity, mode)) < 0)
	
	repair_error_check(res, mode);
	
	return res;
}
