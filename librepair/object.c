/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

extern void reiser4_object_create_base(reiser4_fs_t *fs,
    reiser4_object_t *parent, reiser4_object_t *object, object_hint_t *hint);

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

    aal_assert("vpf-1064", fs != NULL);
    aal_assert("vpf-1065", parent != NULL);
    aal_assert("vpf-1066", hint != NULL);
    aal_assert("vpf-1067", fs->tree != NULL);
    
    /* Allocating the memory for object instance */
    if (!(object = aal_calloc(sizeof(*object), 0)))
	return NULL;
    
    reiser4_object_create_base(fs, parent, object, hint);
    
    if (!(object->entity = plugin_call(hint->plugin->o.object_ops,
	create, fs->tree, parent ? parent->entity : NULL, hint, 
	(place_t *)&object->place)))
    {
	aal_exception_error("Can't create object with oid 0x%llx.",
	    reiser4_key_get_objectid(&object->key));
	goto error_free_object;
    }
    
    reiser4_key_assign(&object->key, &hint->object);
    reiser4_key_string(&object->key, object->name);

    return object;

error_free_object:
    aal_free(object);
    return NULL;
}
