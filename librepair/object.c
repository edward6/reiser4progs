/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

errno_t repair_object_check_struct(reiser4_object_t *obj, uint8_t mode) {
    aal_assert("vpf-1044", obj != NULL);

    return plugin_call(obj->entity->plugin->o.object_ops, check_struct, 
	obj->entity, mode);
}

errno_t repair_object_traverse(reiser4_object_t *object) {
    return 0;
}

reiser4_object_t *repair_object_force_create(reiser4_fs_t *fs, 
    reiser4_object_t *parent, object_hint_t *hint)
{
    reiser4_object_t *object;
    oid_t objectid, locality;

    aal_assert("vpf-1064", fs != NULL);
    aal_assert("vpf-1065", parent != NULL);
    aal_assert("vpf-1066", hint != NULL);
    aal_assert("vpf-1067", fs->tree != NULL);
    
    /* Allocating the memory for object instance */
	if (!(object = aal_calloc(sizeof(*object), 0)))
		return NULL;

	/* Initializing fields and preparing the keys */
	object->fs = fs;
	
	/* 
	  This is the special case. In the case parent is NULL, we are trying to
	  create root directory.
	*/
	if (parent) {
		reiser4_key_assign(&hint->parent, &parent->key);
		objectid = reiser4_oid_allocate(fs->oid);
	} else {
		hint->parent.plugin = fs->tree->key.plugin;
		
		if (reiser4_fs_hyper_key(fs, &hint->parent))
			goto error_free_object;
		
		objectid = reiser4_oid_root_objectid(fs->oid);
	}

	locality = reiser4_key_get_objectid(&hint->parent);
    
	/* Building stat data key of the new object */
	hint->object.plugin = hint->parent.plugin;
	
	reiser4_key_build_generic(&hint->object, KEY_STATDATA_TYPE,
				  locality, objectid, 0);
    
	reiser4_key_assign(&object->key, &hint->object);
	reiser4_key_string(&object->key, object->name);
	
	if (!(object->entity = plugin_call(hint->plugin->o.object_ops,
					   create, fs->tree, parent ?
					   parent->entity : NULL,
					   hint, (place_t *)&object->place)))
	{
		aal_exception_error("Can't create object with oid 0x%llx.", 
				    reiser4_key_get_objectid(&object->key));
		goto error_free_object;
	}

	return object;

 error_free_object:
	aal_free(object);
	return NULL;
}
