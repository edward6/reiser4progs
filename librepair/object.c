/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

errno_t repair_object_check_struct(reiser4_object_t *object, 
    reiser4_plugin_t *plugin, uint8_t mode) 
{
    aal_assert("vpf-1044", object != NULL);
    aal_assert("vpf-1044", plugin != NULL);
    
    object->entity = plugin_call(plugin->o.object_ops, check_struct, 
	&object->info, mode);
    
    if (object->entity == NULL)
       return -EINVAL;
    
    /* FIXME-VITALY: this is probably should be set by plugin. 
     * Together with object->info.parent key. */
    reiser4_key_assign(&object->info.object, &object->info.start.item.key);
    reiser4_key_string(&object->info.object, object->name);

    return 0;
}

errno_t repair_object_check_link(reiser4_object_t *object, 
    reiser4_object_t *parent, uint8_t mode) 
{
    aal_assert("vpf-1044", object != NULL);
    aal_assert("vpf-1098", object->entity != NULL);
    aal_assert("vpf-1099", parent != NULL);    
    aal_assert("vpf-1100", parent->entity != NULL);
    
    return plugin_call(object->entity->plugin->o.object_ops, check_link, 
	&object->info, &parent->info, mode);
}

errno_t repair_object_launch(reiser4_object_t *object) {
    aal_assert("vpf-1097", object != NULL);
    
    reiser4_key_assign(&object->info.object, &object->info.start.item.key);
    reiser4_key_string(&object->info.object, object->name);
    
    return reiser4_object_guess(object);
}

inline void repair_object_init(reiser4_object_t *object, 
    reiser4_tree_t *tree, reiser4_place_t *place, 
    reiser4_key_t *parent, reiser4_key_t *key)
{
    aal_memset(object, 0, sizeof(*object));
    object->info.tree = tree;
    
    if (place)
	aal_memcpy(&object->info.start, place, sizeof(*place));
    
    if (parent)
	aal_memcpy(&object->info.parent, parent, sizeof(*parent));
    
    if (key)
	aal_memcpy(&object->info.object, key, sizeof(*key));
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
reiser4_plugin_t *repair_object_realize(reiser4_object_t *object) {
    lookup_t lookup = PRESENT;
    rid_t pid = INVAL_PID;
    
    aal_assert("vpf-1083", object != NULL); 
    aal_assert("vpf-1084", object->info.tree != NULL); 
    
    if (object->info.object.plugin != NULL) {
	
	/* Realize by specified key, looking up its start place. */
	lookup = reiser4_tree_lookup(object->info.tree, &object->info.object, 
	    LEAF_LEVEL, (reiser4_place_t *)&object->info.start);
    
	if (lookup == FAILED)
	    return NULL;

	/* Even if SD is lost, recognize plugin method will return 0 with 
	 * specified key only if key matches the start key of the object. */
    } 
    
    do {
	reiser4_plugin_t *plugin;
	
	if (lookup != PRESENT)
	    break;
	
	/* The start of the object seems to be found, is it SD? */
	if (reiser4_place_realize((reiser4_place_t *)&object->info.start))
	    return NULL;

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
	if (!(plugin = libreiser4_factory_ifind(OBJECT_PLUGIN_TYPE, pid)))
	    break;
	
	/* Ask the plugin if it realizes the object or not. */
	if (!plugin_call(plugin->o.object_ops, realize, &object->info))
	    return plugin;
    } while (FALSE);
    
    /* Try all plugins to realize the object, choose the more preferable. 
     * FIXME-VITALY: plugins are not sorted yet in the list. */
    return libreiser4_factory_cfind(callback_object_guess, &object->info);
}

errno_t repair_object_traverse(reiser4_object_t *object) {
    reiser4_object_t child;
    entry_hint_t entry;
    errno_t res = 0;

    aal_assert("vpf-1090", object != NULL);
    aal_assert("vpf-1092", object->info.tree != NULL);
    
    repair_object_init(&child, object->info.tree, NULL, &object->info.object, 
	NULL);
    
    while (reiser4_object_readdir(object, &entry)) {
	reiser4_plugin_t *plugin;
	
	/* Some entry was read. Try to detect the object of the paticular plugin
	 * pointed by this entry. */
	
	child.info.object = entry.object;
	
	/* Cannot detect the object plugin, rm the entry. */
	if ((plugin = repair_object_realize(&child)) == NULL)
	    goto child_recovery_problem;
	
	/* FIXME-VITALY: put mode here somehow. */
	if ((res = repair_object_check_struct(&child, plugin, 0 /*MODE*/ )) < 0) {
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

