/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.c -- reiser4 plugin factory.It contains code for loading, unloading
   and finding plugins. */

#include <reiser4/libreiser4.h>

/* Hash table contains all known libreiser4 plugins. */
aal_hash_table_t *plugins;

/* Structure that contains libreiser4 functions available for all plugins to be
   used. */
extern reiser4_core_t core;

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
		aal_error("Can't load another plugin with "
			  "the same label %s.", plug->label);
		return -EINVAL;
	}
	
	/* Check plugin group. It should not be more or equal LAST_ITEM. */
	if (examined->id.group >= LAST_ITEM) {
		aal_error("Plugin %s has invalid group id 0x%x.", 
			  examined->label, examined->id.group);
		return -EINVAL;
	}

	/* Check plugin id, type and group. There should be only one plugin with
	   particular id. */
	if (examined->id.group == plug->id.group &&
	    examined->id.id == plug->id.id &&
	    examined->id.type == plug->id.type)
	{
		aal_error("Plugin %s has the same id as %s.", 
			  examined->label, plug->label);
		return -EINVAL;
	}

	return 0;
}
#endif

/* Initializes plugin and returns it to caller. Calls plugin's init() method,
   etc.*/
static reiser4_plug_t *reiser4_plug_init(plug_class_t *cl) {
	reiser4_plug_t *plug;
	
	if (!cl->init)
		return NULL;

	if (!(plug = cl->init(&core))) {
		aal_warn("Plugin's init() method (%p) "
			 "failed", (void *)cl->init);
		return NULL;
	}

	plug->cl.init = cl->init;
	plug->cl.fini = cl->fini;

	return plug;
}

/* Finalizes a plugin. Mostly calls its fini() function. */
static errno_t reiser4_plug_fini(reiser4_plug_t *plug) {
	errno_t res = 0;
	
	/* Calling plugin fini() method if any. */
	if (plug->cl.fini) {
		if ((res = plug->cl.fini(&core))) {
			aal_warn("Method fini() of plugin "
				 "%s has failed. Error %llx.",
				 plug->label, res);
		}
	}

	plug->cl.init = 0;
	plug->cl.fini = 0;

	return res;
}

/* Loads and initializes plugin by its entry. Also this function makes register
   the plugin in plugins list. */
reiser4_plug_t *reiser4_factory_load(plug_class_t *cl) {
	reiser4_plug_t *plug;

	if (!(plug = reiser4_plug_init(cl)))
		return NULL;
	
#ifndef ENABLE_STAND_ALONE
	if (reiser4_factory_foreach(callback_check_plug, (void *)plug))	{
		aal_warn("Plugin %s will not be attached to "
			 "plugin factory.", plug->label);
		reiser4_factory_unload(plug);
		return NULL;
	}
#endif
	
	aal_hash_table_insert(plugins, &plug->id, plug);
	return plug;
}

/* Unloads plugin and removes it from plugin hash table. */
errno_t reiser4_factory_unload(reiser4_plug_t *plug) {
	aal_assert("umka-1496", plug != NULL);

	reiser4_plug_fini(plug);
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

/* Macro for loading plugin by its name. */
#define __load_plug(name) {                         \
	plug_class_t cl;		            \
		                                    \
	extern plug_init_t __##name##_plug_init;    \
	extern plug_fini_t __##name##_plug_fini;    \
						    \
	cl.init = __##name##_plug_init;          \
	cl.fini = __##name##_plug_fini;          \
                                                    \
	reiser4_factory_load(&cl);               \
}

#ifndef ENABLE_STAND_ALONE
#  define PLUGINS_TABLE_SIZE 128
#else
#  define PLUGINS_TABLE_SIZE 32
#endif
	
/* Initializes all built-in plugins. Other kinds of plugins are not supported
   for now.  */
errno_t reiser4_factory_init(void) {
	aal_assert("umka-3013", plugins == NULL);

	/* Init plugin hash table. */
	if (!(plugins = aal_hash_table_create(PLUGINS_TABLE_SIZE,
					      callback_hash_func,
					      callback_comp_func,
					      NULL, NULL)))
	{
		return -ENOMEM;
	}

	/* Registering all known plugins. */
	__load_plug(format40);

#ifndef ENABLE_STAND_ALONE
	__load_plug(oid40);
	__load_plug(alloc40);
	__load_plug(journal40);
#endif
	
#ifdef ENABLE_R5_HASH
	__load_plug(r5_hash);
#endif

#ifdef ENABLE_TEA_HASH
	__load_plug(tea_hash);
#endif

#ifdef ENABLE_DEG_HASH
	__load_plug(deg_hash);
#endif
	
#ifdef ENABLE_FNV1_HASH
	__load_plug(fnv1_hash);
#endif
	
#ifdef ENABLE_RUPASOV_HASH
	__load_plug(rupasov_hash);
#endif

#ifdef ENABLE_LEXIC_FIBRE
	__load_plug(fibre_lexic);
#endif
	
#ifdef ENABLE_DOT_O_FIBRE
	__load_plug(fibre_dot_o);
#endif
	
#ifdef ENABLE_EXT_1_FIBRE
	__load_plug(fibre_ext_1);
#endif
	
#ifdef ENABLE_EXT_3_FIBRE
	__load_plug(fibre_ext_3);
#endif
	
	__load_plug(sdext_lw);
	__load_plug(sdext_lt);
	__load_plug(sdext_unix);
	__load_plug(sdext_plug);

	__load_plug(cde40);
	__load_plug(stat40);
	__load_plug(tail40);
	__load_plug(extent40);
	__load_plug(nodeptr40);
	__load_plug(bbox40);

#ifdef ENABLE_LARGE_KEYS
	__load_plug(key_large);
#endif
	
#ifdef ENABLE_SHORT_KEYS
	__load_plug(key_short);
#endif
	
	__load_plug(node40);
	__load_plug(dir40);
	__load_plug(reg40);
	
#ifdef ENABLE_SPECIAL
	__load_plug(spl40);
#endif

#ifdef ENABLE_SYMLINKS
	__load_plug(sdext_symlink);
	__load_plug(sym40);
#endif

#ifndef ENABLE_STAND_ALONE
	__load_plug(extents);
	__load_plug(smart);
	__load_plug(tails);
#endif

#ifndef ENABLE_STAND_ALONE
	/* Check if at least one plugin has registered in plugins hash table. If
	   there are no one, plugin factory is considered not successfully
	   initialized.*/
	if (plugins->real == 0) {
                aal_error("There are no valid "
			  "builtin plugins found.");
		aal_hash_table_free(plugins);
                return -EINVAL;
        }
#endif

        return 0;
}

/* Helper function for unloading one plugin. */
static errno_t callback_unload_plug(reiser4_plug_t *plug,
				    void *data)
{
	return reiser4_plug_fini(plug);
}

/* Finalizes plugin factory, by means of unloading the all plugins. */
void reiser4_factory_fini(void) {
	aal_assert("umka-335", plugins != NULL);

	reiser4_factory_foreach(callback_unload_plug, NULL);
	aal_hash_table_free(plugins);
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
/* Helper function for comparing each plugin registered in plugin factory with
   passed one in order to check if they name the same . */
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
#endif

/* Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information. */
errno_t reiser4_factory_foreach(plug_func_t plug_func, void *data) {
	enum_hint_t hint;
	
	aal_assert("umka-3005", plugins != NULL);    
	aal_assert("umka-3006", plug_func != NULL);    

	hint.plug = NULL;
	hint.data = data;
	hint.plug_func = plug_func;

	return aal_hash_table_foreach(plugins,
				      callback_foreach_plug, &hint);
}

/* Finds plugin by its type and id. */
reiser4_plug_t *reiser4_factory_ifind(rid_t type, rid_t id) {
	plug_ident_t ident;
	aal_hash_node_t **node;

	aal_assert("umka-155", plugins != NULL);    

	ident.id = id;
	ident.type = type;
	
	node = aal_hash_table_lookup_node(plugins, &ident);

	if (!node || !(*node))
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

	aal_hash_table_foreach(plugins, callback_foreach_plug, &hint);
	
	return hint.plug;
}

#ifndef ENABLE_STAND_ALONE
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
