/*  Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by 
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
	
	if ((res = plug_call(object->ent->opset.plug[OPSET_OBJ]->o.object_ops,
			     check_struct, object->ent, place_func, 
			     data, mode)) < 0)
		return res;
	
	aal_assert("vpf-1195", mode != RM_BUILD ||
			      !(res & RE_FATAL));
	
	reiser4_key_assign(&object->ent->object, &object_start(object)->key);

	aal_strncpy(object->name, 
		    reiser4_print_key(&object->ent->object, PO_INODE),
		    sizeof(object->name));
	
	return res;
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
	aal_assert("vpf-1640", plug != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	/* Initializing info */
	aal_memset(&info, 0, sizeof(info));

	info.tree = (tree_entity_t *)tree;
	reiser4_key_assign(&info.object, key);

	if (parent)
		reiser4_key_assign(&info.parent, &parent->ent->object);
	
	/* Create the fake object. */
	if (!(object->ent = plug_call(plug->o.object_ops, fake, &info)))
		goto error_close_object;
	
	name = reiser4_print_key(&object->ent->object, PO_INODE);
	aal_strncpy(object->name, name, sizeof(object->name));

	return object;
	
 error_close_object:
	aal_free(object);
	return NULL;
}

static object_entity_t *callback_object_open(object_info_t *info) {
	/* Try to init on the StatData. */
	if (reiser4_object_init(info))
		return INVAL_PTR;

	return plug_call(info->opset.plug[OPSET_OBJ]->o.object_ops,
			 recognize, info);
}

reiser4_object_t *repair_object_open(reiser4_tree_t *tree, 
				     reiser4_object_t *parent,
				     reiser4_place_t *place) 
{
	aal_assert("vpf-1622", place != NULL);
	
	return reiser4_object_form(tree, parent, &place->key, 
				   place, callback_object_open);
}

/* Open the object on the base of given start @key */
reiser4_object_t *repair_object_obtain(reiser4_tree_t *tree,
				       reiser4_object_t *parent,
				       reiser4_key_t *key)
{
	lookup_hint_t hint;
	reiser4_place_t place;

	aal_assert("vpf-1132", tree != NULL);
	aal_assert("vpf-1134", key != NULL);

	hint.key = key;
	hint.level = LEAF_LEVEL;
	hint.collision = NULL;
	
	if (reiser4_tree_lookup(tree, &hint, FIND_EXACT, &place) < 0)
		return INVAL_PTR;
	
	/* Even if ABSENT, pass the found place through object recognize 
	   method to check all possible corruptions. */
	return reiser4_object_form(tree, parent, key, &place, 
				   callback_object_open);
}

/* Checks the attach between @parent and @object */
errno_t repair_object_check_attach(reiser4_object_t *parent, 
				   reiser4_object_t *object, 
				   place_func_t place_func, 
				   void *data, uint8_t mode)
{
	reiser4_plug_t *plug;
	
	aal_assert("vpf-1188", object != NULL);
	aal_assert("vpf-1098", object->ent != NULL);
	aal_assert("vpf-1099", parent != NULL);
	aal_assert("vpf-1100", parent->ent != NULL);
	
	plug = object->ent->opset.plug[OPSET_OBJ];
	
	if (!plug->o.object_ops->check_attach)
		return 0;
	
	return plug_call(plug->o.object_ops, check_attach, object->ent, 
			 parent->ent, place_func, data, mode);
}

errno_t repair_object_mark(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1270", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_refresh(object))) {
		aal_error("Update of the object [%s] failed.",
			  object->name);
		return res;
	}
	
	reiser4_item_set_flag(object_start(object), flag);
	
	return 0;
}

int repair_object_test(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1273", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_refresh(object))) {
		aal_error("Update of the object [%s] failed.",
			  object->name);
		return res;
	}
	
	return reiser4_item_test_flag(object_start(object), flag);
}

errno_t repair_object_clear(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1272", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_refresh(object))) {
		aal_error("Update of the object [%s] failed.",
			  object->name);
		return res;
	}
	
	reiser4_item_clear_flag(object_start(object), flag);
	
	return 0;
}

errno_t repair_object_refresh(reiser4_object_t *object) {
	reiser4_plug_t *plug;
	entry_hint_t entry;
	
	aal_assert("vpf-1271", object != NULL);

	plug = object->ent->opset.plug[OPSET_OBJ];
	
	if (!plug->o.object_ops->lookup)
		return 0;

	switch (plug_call(plug->o.object_ops, lookup, 
			  object->ent, "..", &entry))
	{
	case ABSENT:
		aal_memset(&object->ent->parent, 0, 
			   sizeof(object->ent->parent));
		break;
	case PRESENT:
		plug_call(entry.object.plug->o.key_ops, assign,
			  &object->ent->parent, &entry.object);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Helper function for printing passed @place into @stream. */
static errno_t callback_print_place(reiser4_place_t *place, void *data) {
	aal_stream_t *stream = (aal_stream_t *)data;
	
	repair_item_print(place, stream);
	aal_stream_write(stream, "\n", 1);
	return 0;
}

/* Prints object items into passed stream */
void repair_object_print(reiser4_object_t *object, aal_stream_t *stream) {
	reiser4_object_metadata(object, callback_print_place, stream);
}
