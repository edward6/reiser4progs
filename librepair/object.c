/*  Copyright 2001-2005 by Hans Reiser, licensing governed by 
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
	
	if ((res = plugcall(reiser4_psobj(object), check_struct, 
			    object, place_func, data, mode)) < 0)
	{
		return res;
	}
	
	aal_assert("vpf-1195", mode != RM_BUILD ||
			      !(res & RE_FATAL));
	
	aal_memcpy(&object->info.object, 
		   &object_start(object)->key,
		   sizeof(object->info.object));

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

	aal_assert("vpf-1247", tree != NULL);
	aal_assert("vpf-1248", key != NULL);
	aal_assert("vpf-1640", plug != NULL);

	if (!(object = aal_calloc(sizeof(*object), 0)))
		return INVAL_PTR;

	/* Initializing info */
	aal_memcpy(&object->info.object, key, sizeof(*key));
	
	object->info.tree = (tree_entity_t *)tree;
	object->info.pset.plug[PSET_OBJ] = plug;
	object->info.pset.plug_mask |= (1 << PSET_OBJ);

	if (parent) {
		aal_memcpy(&object->info.parent, 
			   &parent->info.object, 
			   sizeof(object->info.parent));
		
		/* Inherit from the parent. */
		if (plugcall(reiser4_psobj(object), inherit, 
			     &object->info, &parent->info))
		{
			return NULL;
		}
	} else {
		reiser4_pset_root(&object->info);
	}
	
	/* Create the fake object. */
	if (plugcall(reiser4_psobj(object), fake, object)) {
		aal_free(object);
		return NULL;
	}
	
	return object;
}

reiser4_object_t *repair_object_open(reiser4_tree_t *tree, 
				     reiser4_object_t *parent,
				     reiser4_place_t *place) 
{
	reiser4_object_t *object;
	errno_t res;
	
	aal_assert("vpf-1622", place != NULL);
	
	if (!(object = reiser4_object_prep(tree, parent, 
					   &place->key, place)))
	{
		return INVAL_PTR;
	}
	
	if ((res = plugcall(reiser4_psobj(object), recognize, object))) {
		aal_free(object);
		return res < 0 ? INVAL_PTR : NULL;
	}
	
	return object;
}

/* Open the object on the base of given start @key */
reiser4_object_t *repair_object_obtain(reiser4_tree_t *tree,
				       reiser4_object_t *parent,
				       reiser4_key_t *key)
{
	reiser4_object_t *object;
	reiser4_place_t place;
	lookup_hint_t hint;
	errno_t res;

	aal_assert("vpf-1132", tree != NULL);
	aal_assert("vpf-1134", key != NULL);

	hint.key = key;
	hint.level = LEAF_LEVEL;
	hint.collision = NULL;
	
	if (reiser4_tree_lookup(tree, &hint, FIND_EXACT, &place) != PRESENT)
		return NULL;
	
	if (!(object = reiser4_object_prep(tree, parent, key, &place))) {
		/* FIXME: object_init fails to initialize if found item 
		   is not SD, but this is not fatal error. */
		return NULL;
	}
	
	if ((res = plugcall(reiser4_psobj(object), recognize, object))) {
		aal_free(object);
		return res < 0 ? INVAL_PTR : NULL;
	}
	
	return object;
}

/* Checks the attach between @parent and @object */
errno_t repair_object_check_attach(reiser4_object_t *parent, 
				   reiser4_object_t *object, 
				   place_func_t place_func, 
				   void *data, uint8_t mode)
{
	aal_assert("vpf-1188", object != NULL);
	aal_assert("vpf-1099", parent != NULL);
	
	if (!reiser4_psobj(object)->check_attach)
		return 0;
	
	return plugcall(reiser4_psobj(object), check_attach, 
			object, parent, place_func, data, mode);
}

errno_t repair_object_mark(reiser4_object_t *object, uint16_t flag) {
	errno_t res;
	
	aal_assert("vpf-1270", object != NULL);
	
	/* Get the start place. */
	if ((res = reiser4_object_refresh(object))) {
		aal_error("Update of the object [%s] failed.",
			  reiser4_print_inode(&object->info.object));
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
			  reiser4_print_inode(&object->info.object));
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
			  reiser4_print_inode(&object->info.object));
		return res;
	}
	
	reiser4_item_clear_flag(object_start(object), flag);
	
	return 0;
}

/* This method is intended to refresh all info needed for the futher work 
   with the object. For now it is only updating the parent pointer. 
   Should be called after repair_object_obtain, repair_object_check_sytict. */
errno_t repair_object_refresh(reiser4_object_t *object) {
	entry_hint_t entry;
	
	aal_assert("vpf-1271", object != NULL);

	if (!reiser4_psobj(object)->lookup)
		return 0;

	switch (plugcall(reiser4_psobj(object), lookup, object, "..", &entry))
	{
	case ABSENT:
		aal_memset(&object->info.parent, 0, 
			   sizeof(object->info.parent));
		break;
	case PRESENT:
		aal_memcpy(&object->info.parent, 
			   &entry.object, sizeof(entry.object));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Helper function for printing passed @place into @stream. */
static errno_t cb_print_place(reiser4_place_t *place, void *data) {
	aal_stream_t *stream = (aal_stream_t *)data;
	
	repair_item_print(place, stream);
	aal_stream_write(stream, "\n", 1);
	return 0;
}

/* Prints object items into passed stream */
void repair_object_print(reiser4_object_t *object, aal_stream_t *stream) {
	reiser4_object_metadata(object, cb_print_place, stream);
}
