/* 
    librepair/object.c - Object consystency recovery code.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include <repair/object.h>

extern void reiser4_object_create_base(reiser4_fs_t *fs,
    reiser4_object_t *parent, reiser4_object_t *object, object_hint_t *hint);

errno_t repair_object_check_struct(repair_object_t *hint, uint8_t mode) {
    aal_assert("vpf-1044", hint != NULL);
    aal_assert("vpf-1044", hint->plugin != NULL);
    
    return plugin_call(hint->plugin->o.object_ops, check_struct,
	hint, mode);
}

#if 0
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
#endif

/* Helper callback for probing passed @plugin. 
 * FIXME-VITALY: for now it returns the first matched plugin, it should be 
 * changed if plugins are not sorted in some order of adventages of recovery. */
static bool_t callback_object_guess(reiser4_plugin_t *plugin, void *data)
{
    repair_object_t *hint;
    
    /* We are interested only in object plugins here */
    if (plugin->h.type != OBJECT_PLUGIN_TYPE)
	return FALSE;
    
    hint = (repair_object_t *)data;
    
    /* Try to realize the object as an instance of this plugin. */
    return plugin_call(plugin->o.object_ops, realize, hint) ? FALSE : TRUE;
}

void repair_object_init(repair_object_t *hint, reiser4_tree_t *tree) {
    aal_assert("vpf-1076", hint != NULL);
    aal_assert("vpf-1077", tree != NULL);
    
    aal_memset(hint, 0, sizeof(*hint));
    hint->tree = tree;
}

/* Try to recognized the object plugin by the given @hint->place. */
static errno_t repair_object_pullout(repair_object_t *hint, 
    reiser4_place_t *place, reiser4_key_t *key) 
{
    lookup_t lookup = PRESENT;
    rid_t pid = INVAL_PID;
    
    aal_assert("vpf-1083", hint != NULL); 
    aal_assert("vpf-1084", hint->tree != NULL); 
    
    aal_assert("vpf-1085", (key != NULL && key->plugin != NULL) || 
	place != NULL);

    if (key != NULL) {
	/* Pull out by key. */
	
	hint->key = key;
	aal_memset(&hint->place, 0, sizeof(*place));
	
	/* Looking for place to insert directory stat data */
	lookup = reiser4_tree_lookup(hint->tree, hint->key, LEAF_LEVEL, 
	    &hint->place);
    
	if (lookup == FAILED)
	    return -EINVAL;
    } else {
	/* Pull out by place. */
	hint->key = NULL;
	
	aal_memcpy(&hint->place, place, sizeof(*place));
    }
    
    do {
	if (lookup != PRESENT)
	    break;
	
	/* The start of the object seems to be found, is it SD? */
	if (reiser4_place_realize(&hint->place))	    
	    return -EINVAL;

	/* If it is stat data, try to get object plugin from it. */
	if (!reiser4_item_statdata(&hint->place))
	    break;

	/* This is an SD found, try to get object plugin id from it. */
	if (hint->place.item.plugin->o.item_ops->get_plugid) {
	    pid = hint->place.item.plugin->o.item_ops->get_plugid(
		&hint->place.item, OBJECT_PLUGIN_TYPE);
	}
	
	/* Try to realize the object with this plugin. */
	if (pid == INVAL_PID)
	    break;
	
	/* Plugin id was obtained from the SD. Get the plugin. */
	if (!(hint->plugin = libreiser4_factory_ifind(
	    OBJECT_PLUGIN_TYPE, pid)))
	{
	    break;
	}
	
	/* Ask the plugin if it realizes the object or not. */
	if (!plugin_call(hint->plugin->o.object_ops, realize, hint))
	    return 0;
    } while (FALSE);
    
    /* Try all plugins to realize the object, choose the more preferable. 
     * FIXME-VITALY: plugins are not sorted yet in the list. */
    hint->plugin = libreiser4_factory_cfind(callback_object_guess, hint);
    
    return hint->plugin == NULL ? -EINVAL : 0;
}

/* Realize the object plugin by the given @place. */
errno_t repair_object_realize(repair_object_t *hint, reiser4_place_t *place) {
    return repair_object_pullout(hint, place, NULL);
}

/* Realize the object plugin by the given @key. */
errno_t repair_object_launch(repair_object_t *hint, reiser4_key_t *key) {
    return repair_object_pullout(hint, NULL, key);
}

reiser4_object_t *repair_object_open(repair_object_t *hint) {
    reiser4_object_t *object;
    
    aal_assert("vpf-1087", hint != NULL);
    aal_assert("vpf-1088", hint->plugin != NULL);
    aal_assert("vpf-1089", hint->tree != NULL);
    
    if (!(object = aal_calloc(sizeof(*object), 0)))
	return NULL;
    
    object->fs = hint->tree->fs;
    
#ifndef ENABLE_STAND_ALONE
    reiser4_key_string(&object->key, object->name);
#endif
    
    aal_memcpy(&object->place, &hint->place, sizeof(hint->place));
    reiser4_key_assign(&object->key, &object->place.item.key);
    
    object->entity = plugin_call(hint->plugin->o.object_ops, open,
	(void *)hint->tree, (void *)&object->place);
    
    if (object->entity == NULL)
	goto error_free_object;
    
    return object;
    
error_free_object:
    aal_free(object);
    return NULL;
}

errno_t repair_object_traverse(reiser4_object_t *object) {
    reiser4_object_t *child;
    repair_object_t hint;
    entry_hint_t entry;
    errno_t res = 0;

    aal_assert("vpf-1090", object != NULL);
    aal_assert("vpf-1091", object->fs != NULL);
    aal_assert("vpf-1092", object->fs->tree != NULL);
    
    repair_object_init(&hint, object->fs->tree);
    
    while (reiser4_object_readdir(object, &entry)) {
	/* Some entry was read. Try to detect the object of the paticular plugin
	 * pointed by this entry. */
	
	if (repair_object_launch(&hint, &entry.object)) {
	    /* Some problems with the object recovery appeared, rm the entry. */
	
	    if ((res = reiser4_object_rem_entry(object, &entry))) {
		aal_exception_error("Semantic traverse failed to remove the "
		    "entry %k (%s) pointing to %k.", &entry.offset, entry.name,
		    &entry.object);
		return res;
	    }

	    continue;
	}
	
	/* FIXME-VITALY: put mode here somehow. */
	res = repair_object_check_struct(&hint, 0/* mode */);
	
	/* Some plugin was detected, check it if needed. */
	if (res < 0) {
	    aal_exception_error("Check of the object pointed by %k from the "
		"%k (%s) failed.", &entry.object, &entry.offset, entry.name);
	    return res;
	} else if (res > 0)
	    continue;
	
	/* Open the object and travserse it. */
	if ((child = repair_object_open(&hint)) == NULL) {
	    aal_exception_error("Node %llu, item %u: Object openning after "
		"recovering failed for %k.", hint.place.node->blk, 
		hint.place.pos.item, &hint.place.item.key);
	    return -EINVAL;
	}
	
	if (repair_object_traverse(child))
	    goto error_child_close;

	reiser4_object_close(child);
    }
    
    return 0;
    
error_child_close:
    reiser4_object_close(child);
    return res;
}
