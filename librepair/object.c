/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

errno_t repair_object_check_struct(reiser4_object_t *object, uint8_t mode) {
    aal_assert("vpf-1044", object != NULL);
    aal_assert("vpf-1044", object->entity->plugin != NULL);
    
    object->entity = plugin_call(object->entity->plugin->o.object_ops, 
	check_struct, &object->info, mode);
    
    if (object->entity == NULL)
       return -EINVAL;
    
    reiser4_key_assign(&object->info.object, &object->info.start.item.key);
    reiser4_key_string(&object->info.object, object->name);

    return 0;
}

errno_t repair_object_launch(reiser4_object_t *object) {
    aal_assert("vpf-1097", object != NULL);
    
    reiser4_key_assign(&object->info.object, &object->info.start.item.key);
    reiser4_key_string(&object->info.object, object->name);
    
    return reiser4_object_guess(object);
}

/* Helper callback for probing passed @plugin. 
 * FIXME-VITALY: for now it returns the first matched plugin, it should be 
 * changed if plugins are not sorted in some order of adventages of recovery. */
static bool_t callback_object_guess(reiser4_plugin_t *plugin, void *data)
{
    object_info_t *info;
    
    /* We are interested only in object plugins here */
    if (plugin->h.type != OBJECT_PLUGIN_TYPE)
	return FALSE;
    
    info = (object_info_t *)data;
    
    /* Try to realize the object as an instance of this plugin. */
    return plugin_call(plugin->o.object_ops, realize, info) ? FALSE : TRUE;
}

/* Try to recognized the object plugin by the given @object->info. 
 * Either object->info.place must be valid or object->info.object and
 * object->info.parent keys. */
errno_t repair_object_realize(reiser4_object_t *object) {
    lookup_t lookup = PRESENT;
    rid_t pid = INVAL_PID;
    
    aal_assert("vpf-1083", object != NULL); 
    aal_assert("vpf-1084", object->info.tree != NULL); 
    
    if (object->info.object.plugin != NULL) {
	
	/* Realize by specified key, looking up its start place. */
	lookup = reiser4_tree_lookup(object->info.tree, &object->info.object, 
	    LEAF_LEVEL, (reiser4_place_t *)&object->info.start);
    
	if (lookup == FAILED)
	    return -EINVAL;
    } 
    
    while (TRUE) {
	if (lookup != PRESENT)
	    break;
	
	/* The start of the object seems to be found, is it SD? */
	if (reiser4_place_realize((reiser4_place_t *)&object->info.start))
	    return -EINVAL;

	/* If it is stat data, try to get object plugin from it. */
	if (!reiser4_item_statdata((reiser4_place_t *)&object->info.start))
	    break;

	/* This is an SD found, try to get object plugin id from it. */
	if (object->info.start.item.plugin->o.item_ops->get_plugid) {
	    pid = object->info.start.item.plugin->o.item_ops->get_plugid(
		&object->info.start.item, OBJECT_PLUGIN_TYPE);
	}
	
	/* Try to realize the object with this plugin. */
	if (pid == INVAL_PID)
	    break;
	
	/* Plugin id was obtained from the SD. Get the plugin. */
	if (!(object->entity->plugin = libreiser4_factory_ifind(
	    OBJECT_PLUGIN_TYPE, pid)))
	    break;
	
	/* Ask the plugin if it realizes the object or not. */
	if (!plugin_call(object->entity->plugin->o.object_ops, realize, 
	    &object->info))
	    return 0;
    };
    
    /* Try all plugins to realize the object, choose the more preferable. 
     * FIXME-VITALY: plugins are not sorted yet in the list. */
    object->entity->plugin = libreiser4_factory_cfind(callback_object_guess, 
	&object->info);
    
    return object->entity->plugin == NULL ? -EINVAL : 0;
}

errno_t repair_object_traverse(reiser4_object_t *object) {
    reiser4_object_t child;
    entry_hint_t entry;
    errno_t res = 0;

    aal_assert("vpf-1090", object != NULL);
    aal_assert("vpf-1092", object->info.tree != NULL);
    
    aal_memset(&child, 0, sizeof(child));
    child.info.tree = object->info.tree;
    child.info.parent = object->info.object;
	
    while (reiser4_object_readdir(object, &entry)) {
	/* Some entry was read. Try to detect the object of the paticular plugin
	 * pointed by this entry. */
	
	child.info.object = entry.object;
	
	/* Cannot detect the object plugin, rm the entry. */
	if (repair_object_realize(&child)) 
	    goto child_recovery_problem;
	
	/* FIXME-VITALY: put mode here somehow. */
	if ((res = repair_object_check_struct(&child, 0/* mode */)) < 0) {
	    aal_exception_error("Check of the object pointed by %k from the "
		"%k (%s) failed.", &entry.object, &entry.offset, entry.name);
	    return res;
	} 
	
	/* If unrecoverable corruptions were found, rm the entry. */
	if (res > 0) 
	    goto child_recovery_problem;
	
	if ((res = repair_object_traverse(&child)))
	    return res;

	plugin_call(child.entity->plugin->o.object_ops, close, child.entity);
	
	continue;
	
    child_recovery_problem:
	if ((res = reiser4_object_rem_entry(object, &entry))) {
	    aal_exception_error("Semantic traverse failed to remove the "
		"entry %k (%s) pointing to %k.", &entry.offset, entry.name,
		&entry.object);
	    return res;
	}
    }
    
    return 0;
}

#if 0
extern void reiser4_object_create_base(reiser4_fs_t *fs,
    reiser4_object_t *parent, reiser4_object_t *object);

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

errno_t repair_object_open(reiser4_object_t *object) {
    aal_assert("vpf-1087", object != NULL);
    aal_assert("vpf-1088", object->info.tree != NULL);
    aal_assert("vpf-1096", object->entity->plugin != NULL);
    
#ifndef ENABLE_STAND_ALONE
    reiser4_key_string(&object->info.object, object->name);
#endif
    
    reiser4_key_assign(&object->info.object, &object->info.start.item.key);
    
    object->entity = plugin_call(object->entity->plugin->o.object_ops, open,
	&object->info.tree, &object->info.start);
    
    return object->entity == NULL ? -EINVAL : 0;
}
#endif

