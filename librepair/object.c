/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

/* Checks if the specified place can be the start of an object. */
bool_t repair_object_can_begin(reiser4_place_t *place) {
    aal_assert("vpf-1033", place != NULL);

    if (reiser4_place_realize(place))
	return FALSE;

    return reiser4_key_get_offset(&place->item.key) == 0;
}

errno_t repair_object_check_struct(reiser4_object_t *obj, uint8_t mode) {
    aal_assert("vpf-1044", obj != NULL);

    return plugin_call(obj->entity->plugin->o.object_ops, check_struct, 
	obj->entity, mode);
}

