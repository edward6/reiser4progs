/*  Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by 
    reiser4progs/COPYING.
    
    librepair/object.c - Object consystency recovery code. */

#include <repair/object.h>
#include <repair/item.h>


/* Check the semantic structure of the object. Mark all items as CHECKED. */
errno_t repair_object_check_struct(reiser4_object_t *object,
				   place_func_t place_func,
				   region_func_t region_func,
				   uint8_t mode, void *data) 
{
	errno_t res;
	
	aal_assert("vpf-1044", object != NULL);
	
	if ((res = plug_call(object->entity->plug->o.object_ops, 
			     check_struct, object->entity, place_func,
			     region_func, data, mode)) < 0)
		return res;
	
	repair_error_check(res, mode);
	aal_assert("vpf-1195", mode != RM_BUILD ||
			      !(res & RE_FATAL));
	
	reiser4_key_assign(&object->info->object, &object->info->start.key);

	aal_strncpy(object->name, 
		    reiser4_print_key(&object->info->object, PO_INO),
		    sizeof(object->name));
	
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
				   object->info);
	
	if (object->entity != NULL && object->entity != INVAL_PTR) {
		plug_call(plug->o.object_ops, close, object->entity);
		return TRUE;
	}
	
	return FALSE;
}

/* FIXME-UMKA->VITALY: Here apparently should be used resier4_object_realize()
   as it is the same as this one. Also, @parent param shpuld be added here. */
reiser4_object_t *repair_object_realize(reiser4_tree_t *tree, 
					reiser4_object_t *parent,
					reiser4_place_t *place)
{
/*	reiser4_plug_t *plug;
	reiser4_object_t *object;

	aal_assert("vpf-1198", tree != NULL);
	aal_assert("vpf-1199", place != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;
    
	object->info->tree = tree;
	
	aal_memcpy(object_start(object), place, sizeof(*place));
	
	if (reiser4_object_guess(object, callback_object_realize))
		goto error_free_object;
	
	reiser4_key_assign(&object->info.object,
			   &object->info.start.key);
	
	aal_strncpy(object->name, 
		    reiser4_print_key(&object->info.object, PO_INO),
		    sizeof(object->name));
	
	return object;
	
 error_free_object:
	aal_free(object);*/
	return NULL;
}

/* Open the object on the base of given start @key */
reiser4_object_t *repair_object_launch(reiser4_tree_t *tree,
				       reiser4_object_t *parent,
				       reiser4_key_t *key, 
				       bool_t only)
{
	reiser4_object_t *object;
	reiser4_place_t place;
	reiser4_plug_t *plug;
	object_info_t info;
	lookup_t lookup;
	
	aal_assert("vpf-1132", tree != NULL);
	aal_assert("vpf-1134", key != NULL);
	
	if ((lookup = reiser4_tree_lookup(tree, key, LEAF_LEVEL, 
					  &place)) == FAILED)
		return INVAL_PTR;
	
	if (lookup == PRESENT)
		/* If the pointed item is found, try to realize it.
		   @parent probably should be passed here. */
		return repair_object_realize(tree, parent, &place);
	
	/* ABSENT. Try to realize the object. */
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	info.tree = tree;
	info.object = *key;
	info.start = *(place_t *)&place;

	if (parent)
	   	info.parent = parent->info->object;

	plug = reiser4_factory_cfind(callback_object_realize, &info);
	
	if (plug == NULL)
		goto error_close_object;

	object->entity = plug_call(plug->o.object_ops, realize, &info);
	
	aal_assert("vpf-1196", object->entity != NULL && 
			       object->entity != INVAL_PTR);
	
	aal_strncpy(object->name, 
		    reiser4_print_key(&object->info->object, PO_INO),
		    sizeof(object->name));

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
