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
	
	if ((res = plug_call(object->entity->plug->o.object_ops, check_struct,
			     object->entity, place_func, data, mode)) < 0)
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
static bool_t callback_object_recognize(reiser4_plug_t *plug, void *data) {
	reiser4_object_t *object;
	
	/* We are interested only in object plugins here */
	if (plug->id.type != OBJECT_PLUG_TYPE)
		return FALSE;
	
	object = (reiser4_object_t *)data;
	
	/* Try to recognize the object as an instance of this plugin. */
	object->entity = plug_call(plug->o.object_ops, recognize, 
				   object->info);
	
	return (object->entity == NULL || object->entity == INVAL_PTR) ?
		FALSE : TRUE;
}

static errno_t repair_object_init(reiser4_object_t *object,
				  reiser4_object_t *parent)
{
	reiser4_plug_t *plug;
	
	plug = reiser4_factory_cfind(callback_object_recognize, object);

	return plug == NULL ? -EINVAL : 0;
}

reiser4_object_t *repair_object_recognize(reiser4_tree_t *tree, 
					  reiser4_object_t *parent,
					  reiser4_place_t *place) 
{
	return reiser4_object_guess(tree, parent, NULL, place,
				    repair_object_init);
}

/* Create the fake object--needed for "/" and "lost+found" recovery when SD 
   is corrupted and not directory plugin gets realized. */
reiser4_object_t *repair_object_fake(reiser4_tree_t *tree, 
				     reiser4_object_t *parent,
				     reiser4_key_t *key,
				     reiser4_plug_t *plug) 
{
	reiser4_object_t *object;
	object_info_t info;
	char *name;

	aal_assert("vpf-1247", tree != NULL);
	aal_assert("vpf-1248", key != NULL);
	aal_assert("vpf-1249", plug != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	/* Initializing info */
	aal_memset(&info, 0, sizeof(info));
	info.tree = tree;
	reiser4_key_assign(&info.object, key);
	
	if (parent)
		reiser4_key_assign(&info.parent, &parent->info->object);
	
	/* Create the fake object. */
	if (!(object->entity = plug_call(plug->o.object_ops, fake, &info)))
		goto error_close_object;
	
	object->info = &object->entity->info;
	
	name = reiser4_print_key(&object->info->object, PO_INO);
	aal_strncpy(object->name, name, sizeof(object->name));

	return object;
	
 error_close_object:
	aal_free(object);
	return NULL;
}

/* Open the object on the base of given start @key */
reiser4_object_t *repair_object_launch(reiser4_tree_t *tree,
				       reiser4_object_t *parent,
				       reiser4_key_t *key)
{
	reiser4_place_t place;

	aal_assert("vpf-1132", tree != NULL);
	aal_assert("vpf-1134", key != NULL);
	
	if (reiser4_tree_lookup(tree, key, LEAF_LEVEL, 
				FIND_EXACT, &place) < 0)
		return INVAL_PTR;
	
	/* Even if place is found, pass it through object recognize 
	   method to check all possible corruptions. */
	return reiser4_object_guess(tree, parent, key, &place, 
				    repair_object_init);
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

errno_t repair_object_mark(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1270", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_stat(object))) {
		aal_exception_error("Update of the object [%s] failed.",
				    object->name);
		return res;
	}
	
	repair_item_set_flag(object_start(object), flag);
	
	return 0;
}

int repair_object_test(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1273", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_stat(object))) {
		aal_exception_error("Update of the object [%s] failed.",
				    object->name);
		return res;
	}
	
	return repair_item_test_flag(object_start(object), flag);
}

errno_t repair_object_clear(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1272", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_stat(object))) {
		aal_exception_error("Update of the object [%s] failed.",
				    object->name);
		return res;
	}
	
	repair_item_clear_flag(object_start(object), flag);
	
	return 0;
}

errno_t repair_object_form(reiser4_object_t *object) {
	aal_assert("vpf-1271", object != NULL);

	if (!object->entity->plug->o.object_ops->form)
		return 0;

	return plug_call(object->entity->plug->o.object_ops, 
			 form, object->entity);
}
