/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

/* Check the semantic structure of the object. Mark all items as CHECKED. */
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

errno_t repair_object_traverse(reiser4_object_t *object, traverse_func_t func, 
    void *data) 
{
    entry_hint_t entry;
    errno_t res = 0;

    aal_assert("vpf-1090", object != NULL);
    aal_assert("vpf-1092", object->info.tree != NULL);
    aal_assert("vpf-1103", func != NULL);
    
    while (reiser4_object_readdir(object, &entry)) {
	reiser4_object_t *child;
	
	/* Some entry was read. Try to detect the object of the paticular plugin
	 * pointed by this entry. */
	
	if ((res = func(object, &child, &entry, data)) < 0)
	    return res;
	
	if (res > 0)
	    continue;
	
	if ((res = repair_object_traverse(child, func, data)))
	    return res;
	
	reiser4_object_close(child); 
    }
    
    return 0;
}

/* Check '..' entry of directories. 
 * Fix the parent pointer if needed, mark REACHABLE if the parent pointer in 
 * the object matches the parent pointer, nlink++. */
errno_t repair_object_check_link(reiser4_object_t *object, 
    reiser4_object_t *parent, uint8_t mode) 
{
    aal_assert("vpf-1044", object != NULL);
    aal_assert("vpf-1098", object->entity != NULL);
    aal_assert("vpf-1099", parent != NULL);
    aal_assert("vpf-1100", parent->entity != NULL);
    
    return plugin_call(object->entity->plugin->o.object_ops, check_link, 
	object->entity, parent->entity, mode);
}
