/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.c -- reiser4 plugin factory.It contains code for loading, unloading
   and finding plugins. */

#include <reiser4/reiser4.h>

uint32_t registered;
#define MAX_PLUGINS 30

/* Hash table contains all known libreiser4 plugins. */
aal_hash_table_t *plugins;

/* Structure contains libreiser4 functions available for plugins. */
extern reiser4_core_t core;
static plug_desc_t builtins[MAX_PLUGINS];

#ifndef ENABLE_STAND_ALONE
/* Helper callback for checking plugin validness. It if called for each plugin
   in order to compare its characteristics with characteristics of new
   registered one. */
static errno_t callback_check_plug(reiser4_plug_t *plug,
				   void *data)
{
	reiser4_plug_t *examined = (reiser4_plug_t *)data;

	if (examined == plug)
		return 0;

	/* Check plugin labels. They should not be the same. */
	if (!aal_strncmp(examined->label, plug->label,
			 PLUG_MAX_LABEL))
	{
		aal_exception_error("Plugin %s has the same label "
				    "as %s.", examined->cl.location,
				    plug->cl.location);
		return -EINVAL;
	}
	
	/* Check plugin group. It should not be more or equal LAST_ITEM. */
	if (examined->id.group >= LAST_ITEM) {
		aal_exception_error("Plugin %s has invalid group id "
				    "0x%x.", examined->cl.location,
				    examined->id.group);
		return -EINVAL;
	}

	/* Check plugin id, type and group. There should be only one plugin with
	   particular id. */
	if (examined->id.group == plug->id.group &&
	    examined->id.id == plug->id.id &&
	    examined->id.type == plug->id.type)
	{
		aal_exception_error("Plugin %s has the same id as "
				    "%s.", examined->cl.location,
				    plug->cl.location);
		return -EINVAL;
	}

	return 0;
}
#endif

/* Loads built-in plugin described by @desc into @class descriptor. */
static errno_t reiser4_plug_open(plug_desc_t *desc,
				 plug_class_t *class)
{

	aal_assert("umka-1431", desc != NULL);
	aal_assert("umka-1432", class != NULL);

#ifndef ENABLE_STAND_ALONE
	aal_snprintf(class->location, sizeof(class->location),
		     "built-in (%p)", desc->init);
#endif

	class->fini = desc->fini;
	class->init = desc->init;
	
	return 0;
}

/* Closes built-in plugins */
static void reiser4_plug_close(plug_class_t *class) {
	class->init = 0;
	class->fini = 0;
}

/* Loads and initializes plugin by its entry. Also this function makes register
   the plugin in plugins list. */
errno_t reiser4_factory_load(plug_desc_t *desc) {
	errno_t res;

	plug_class_t class;
	reiser4_plug_t *plug;

	if (!desc->init)
		return -EINVAL;
		
	/* Open plugin by @desc descriptor. */
	if ((res = reiser4_plug_open(desc, &class)))
		return res;

	if (!(plug = class.init(&core))) {
		aal_exception_warn("Plugin's init() method (%p) "
				   "failed", (void *)class.init);
		return -EINVAL;
	}

	plug->cl = class;
	
#ifndef ENABLE_STAND_ALONE
	if ((res = reiser4_factory_foreach(callback_check_plug,
					   (void *)plug)))
	{
		aal_exception_warn("Plugin %s will not be attached to "
				   "plugin factory.", plug->cl.location);
		goto error_free_plug;
	}
#endif
	
	return aal_hash_table_insert(plugins, &plug->id, plug);

#ifndef ENABLE_STAND_ALONE
 error_free_plug:
	reiser4_factory_unload(plug);
	return res;
#endif
}

/* Unloads plugin and removes it from plugin hash table. */
errno_t reiser4_factory_unload(reiser4_plug_t *plug) {
	plug_class_t *class;
	
	aal_assert("umka-1496", plug != NULL);
	
	class = &plug->cl;

	/* Calling plugin fini() method if any. */
	if (class->fini) {
		errno_t res;
		
		if ((res = class->fini(&core))) {
			aal_exception_warn("Method fini() of plugin "
					   "%s has failed. Error %llx.",
					   plug->label, res);
		}
	}

	/* Remove plugin from plugin hash table. */
	reiser4_plug_close(class);
	aal_hash_table_remove(plugins, &plug->id);

	return 0;
}

/* Helper functions used for calculating hash and for comparing two entries from
   plugin hash table during its modifying. */
static uint64_t callback_hash_func(void *key) {
	return (uint64_t)((plug_ident_t *)key)->type;
}

static int callback_comp_func(void *key1, void *key2,
			      void *data)
{
	return (!ident_equal((plug_ident_t *)key1,
			     (plug_ident_t *)key2));
}

/* This function registers builtin plugin entry points. */
static void reiser4_factory_reg(plug_init_t init,
				plug_fini_t fini)
{
	if (registered >= MAX_PLUGINS)
		registered = 0;
		
	builtins[registered].init = init;
	builtins[registered].fini = fini;

	registered++;
}

/* Macro for direct call to corresponding plugin's symbols. This is needed to
   let linker know, that we need that plugin code. Thus, exactly, this macro
   gets address of plugin init() and fini() methods and registers them in
   corresponding entry of @builtins plugins array . */
#define __register_plug(name) {                     \
	extern plug_init_t __##name##_plug_init;    \
	extern plug_fini_t __##name##_plug_fini;    \
	reiser4_factory_reg(__##name##_plug_init,   \
                            __##name##_plug_fini);  \
}

/* Initializes all built-in plugins. Other kinds of plugins are not supported
   for now.  */
errno_t reiser4_factory_init(void) {
	uint32_t i;

	/* Registering all known plugins. */
	__register_plug(format40);

#ifndef ENABLE_STAND_ALONE
	__register_plug(oid40);
	__register_plug(alloc40);
	__register_plug(journal40);
#endif
	
#ifdef ENABLE_R5_HASH
	__register_plug(r5_hash);
#endif

#ifdef ENABLE_TEA_HASH
	__register_plug(tea_hash);
#endif

#ifdef ENABLE_DEG_HASH
	__register_plug(deg_hash);
#endif
	
#ifdef ENABLE_FNV1_HASH
	__register_plug(fnv1_hash);
#endif
	
#ifdef ENABLE_RUPASOV_HASH
	__register_plug(rupasov_hash);
#endif

	__register_plug(sdext_lw);
	__register_plug(sdext_lt);
	__register_plug(sdext_unix);

#ifdef ENABLE_SYMLINKS
	__register_plug(sdext_symlink);
#endif
	
	__register_plug(cde40);
	__register_plug(stat40);
	__register_plug(tail40);
	__register_plug(extent40);
	__register_plug(nodeptr40);

#ifdef ENABLE_LARGE_KEYS
	__register_plug(key_large);
#endif
	
#ifdef ENABLE_SHORT_KEYS
	__register_plug(key_short);
#endif
	
	__register_plug(node40);
	__register_plug(dir40);
	__register_plug(reg40);
	
#ifdef ENABLE_SPECIAL
	__register_plug(spl40);
#endif

#ifdef ENABLE_SYMLINKS
	__register_plug(sym40);
#endif

#ifndef ENABLE_STAND_ALONE
	__register_plug(extents);
	__register_plug(smart);
	__register_plug(tails);
#endif

	/* Init of plugin hash table. */
	if (!(plugins = aal_hash_table_alloc(callback_hash_func,
					     callback_comp_func,
					     NULL, NULL)))
	{
		return -ENOMEM;
	}

        /* Loads the all builtin plugins. */
	for (i = 0; i < MAX_PLUGINS; i++) {
		if (reiser4_factory_load(&builtins[i]))
                        continue;
	}

#ifndef ENABLE_STAND_ALONE
	if (plugins->real == 0) {
                aal_exception_error("There are no valid built-in "
				    "plugins found.");
		aal_hash_table_free(plugins);
                return -EINVAL;
        }
#endif

        return 0;
}

/* Helper function for unloading one plugin. */
static errno_t callback_unload_plug(void *entry, void *data) {
	aal_hash_node_t *node = (aal_hash_node_t *)entry;
	reiser4_factory_unload((reiser4_plug_t *)node->value);
	return 0;
}

/* Finalizes plugin factory, by means of unloading the all plugins. */
void reiser4_factory_fini(void) {
	aal_assert("umka-335", plugins != NULL);

	/* Unloading all registered plugins. */
	aal_hash_table_foreach(plugins, callback_unload_plug, NULL);
	aal_hash_table_free(plugins);

	plugins = NULL;
	registered = 0;
}


/* Plugin hash table enumeration stuff. */
struct enum_hint {
	void *data;
	reiser4_plug_t *plug;
	plug_func_t plug_func;
};

typedef struct enum_hint enum_hint_t;

/* Helper function for calling passed plug_func() for each plugin from passed
   plugin hash table. */
static errno_t callback_foreach_plug(void *entry, void *data) {
	errno_t res;
	enum_hint_t *hint;
	reiser4_plug_t *plug;
	aal_hash_node_t *node;

	hint = (enum_hint_t *)data;
	node = (aal_hash_node_t *)entry;
	plug = (reiser4_plug_t *)node->value;

	if ((res = hint->plug_func(plug, hint->data))) {
		hint->plug = plug;
		return res;
	}

	return 0;
}

#ifndef ENABLE_STAND_ALONE
/* Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information. */
errno_t reiser4_factory_foreach(plug_func_t plug_func, void *data) {
	enum_hint_t hint;
	
	aal_assert("umka-3005", plugins != NULL);    
	aal_assert("umka-3006", plug_func != NULL);    

	hint.plug = NULL;
	hint.data = data;
	hint.plug_func = plug_func;

	return aal_hash_table_foreach(plugins, callback_foreach_plug, &hint);
}
#endif

/* Finds plugin by its type and id. */
reiser4_plug_t *reiser4_factory_ifind(rid_t type, rid_t id) {
	plug_ident_t ident;
	aal_hash_node_t **node;

	aal_assert("umka-155", plugins != NULL);    

	ident.id = id;
	ident.type = type;
	
	if (!(node = aal_hash_table_lookup_node(plugins, &ident)))
		return NULL;
	
	return (reiser4_plug_t *)(*node)->value;
}

/* Finds plugin by variable criterios implemented by passed @plug_func. */
reiser4_plug_t *reiser4_factory_cfind(plug_func_t plug_func, void *data) {
	enum_hint_t hint;
	
	aal_assert("umka-155", plugins != NULL);    
	aal_assert("umka-899", plug_func != NULL);    

	hint.data = data;
	hint.plug = NULL;
	hint.plug_func = plug_func;

	aal_hash_table_foreach(plugins, callback_foreach_plug,
			       &hint);
	
	return hint.plug;
}

#ifndef ENABLE_STAND_ALONE
/* Helper function for comparing each plugin hash table entry with needed
   plugin name. */
static errno_t callback_nfind_plug(void *entry, void *data) {
	enum_hint_t *hint = (enum_hint_t *)data;
	aal_hash_node_t *node = (aal_hash_node_t *)entry;
	reiser4_plug_t *plug = (reiser4_plug_t *)node->value;

	if (!aal_strncmp(plug->label, hint->data,
			 sizeof(plug->label)))
	{
		hint->plug = plug;
		return 1;
	}

	return 0;
}

/* Makes search for plugin by @name. */
reiser4_plug_t *reiser4_factory_nfind(char *name) {
	enum_hint_t hint;

	aal_assert("vpf-156", name != NULL);    
       
	hint.plug = NULL;
	hint.data = name;

	aal_hash_table_foreach(plugins, callback_nfind_plug,
			       &hint);
	
	return hint.plug;
}
#endif
