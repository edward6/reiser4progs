/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.c -- reiser4 plugin factory.It contains code for loading, unloading
   and finding plugins. */

#include <reiser4/libreiser4.h>

/* Hash table contains all known libreiser4 plugins. */
reiser4_plug_t **plugins;

static uint8_t plugs_max[LAST_PLUG_TYPE + 1] = {
	[OBJECT_PLUG_TYPE]        = OBJECT_LAST_ID,
	[ITEM_PLUG_TYPE]          = ITEM_LAST_ID,
	[NODE_PLUG_TYPE]          = NODE_LAST_ID,
	[HASH_PLUG_TYPE]          = HASH_LAST_ID,
	[FIBRE_PLUG_TYPE]	  = FIBRE_LAST_ID,
	[POLICY_PLUG_TYPE]        = TAIL_LAST_ID,
	[SDEXT_PLUG_TYPE]         = SDEXT_LAST_ID,
	[FORMAT_PLUG_TYPE]        = FORMAT_LAST_ID,
	[OID_PLUG_TYPE]           = OID_LAST_ID,
	[ALLOC_PLUG_TYPE]         = ALLOC_LAST_ID,
	[JOURNAL_PLUG_TYPE]       = JOURNAL_LAST_ID,
	[KEY_PLUG_TYPE]           = KEY_LAST_ID,
	[PARAM_PLUG_TYPE]         = 0,
	[LAST_PLUG_TYPE]	  = 0,
};

/* Structure that contains libreiser4 functions available for all plugins to be
   used. */
extern reiser4_core_t core;

#ifndef ENABLE_MINIMAL
/* Helper callback for checking plugin validness. It if called for each plugin
   in order to compare its characteristics with characteristics of new
   registered one. */
static errno_t cb_check_plug(reiser4_plug_t *plug, void *data) {
	reiser4_plug_t *examined = (reiser4_plug_t *)data;

	if (!plug || examined == plug)
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
		aal_error("Plugin's init() method (%p) "
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
	
	if (!plug) return 0;
	
	/* Calling plugin fini() method if any. */
	if (plug->cl.fini) {
		if ((res = plug->cl.fini(&core))) {
			aal_error("Method fini() of plugin %s has "
				  "failed. Error %llx.", plug->label, res);
		}
	}

	plug->cl.init = 0;
	plug->cl.fini = 0;

	return res;
}

/* Helper functions used for calculating hash and for comparing two entries from
   plugin hash table during its modifying. */
#define plug_hash_func(type, id) (plugs_max[type] + (id))

/* Unloads plugin and removes it from plugin hash table. */
static errno_t reiser4_factory_unload(reiser4_plug_t *plug) {
	aal_assert("umka-1496", plug != NULL);

	plugins[plug_hash_func(plug->id.type, plug->id.id)] = NULL;
	reiser4_plug_fini(plug);
	
	return 0;
}

/* Loads and initializes plugin by its entry. Also this function makes register
   the plugin in plugins list. */
reiser4_plug_t *reiser4_factory_load(plug_class_t *cl) {
	reiser4_plug_t *plug;

	if (!(plug = reiser4_plug_init(cl)))
		return NULL;
	
#ifndef ENABLE_MINIMAL
	if (reiser4_factory_foreach(cb_check_plug, (void *)plug)) {
		aal_error("Plugin %s will not be attached to "
			  "plugin factory.", plug->label);
		reiser4_factory_unload(plug);
		return NULL;
	}
#endif
	
	plugins[plug_hash_func(plug->id.type, plug->id.id)] = plug;
	return plug;
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

#ifndef ENABLE_MINIMAL
#  define PLUGINS_TABLE_SIZE 128
#else
#  define PLUGINS_TABLE_SIZE 32
#endif
	
/* Initializes all built-in plugins. Other kinds of plugins are not supported
   for now.  */
errno_t reiser4_factory_init(void) {
	uint8_t prev, max;
	int i;
	
	prev = 0;
	/* Init plugin hash table. */
	for (i = 0; i <= LAST_PLUG_TYPE; i++) {
		max = plugs_max[i];
		if (i == 0)
			plugs_max[i] = 0;
		else
			plugs_max[i] = plugs_max[i - 1] + prev;
		
		prev = max;
	}
	
	plugins = aal_calloc((plugs_max[LAST_PLUG_TYPE]) * sizeof(*plugins), 0);
	
	/* Registering all known plugins. */
	__load_plug(format40);

#ifndef ENABLE_MINIMAL
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
#ifndef ENABLE_MINIMAL
	__load_plug(sdext_crypto);
#endif

	__load_plug(cde40);
	__load_plug(stat40);
	__load_plug(plain40);
#ifndef ENABLE_MINIMAL
	__load_plug(ctail40);
#endif
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

#ifndef ENABLE_MINIMAL
	__load_plug(ccreg40);
#endif

#ifdef ENABLE_SYMLINKS
	__load_plug(sdext_symlink);
	__load_plug(sym40);
#endif

#ifndef ENABLE_MINIMAL
	__load_plug(extents);
	__load_plug(smart);
	__load_plug(tails);
#endif

#ifndef ENABLE_MINIMAL
	/* Check if at least one plugin has registered in plugins hash table. If
	   there are no one, plugin factory is considered not successfully
	   initialized.*/
#endif

        return 0;
}

/* Helper function for unloading one plugin. */
static errno_t cb_unload_plug(reiser4_plug_t *plug, void *data) {
	return plug ? reiser4_plug_fini(plug) : 0;
}

/* Finalizes plugin factory, by means of unloading the all plugins. */
void reiser4_factory_fini(void) {
	reiser4_factory_foreach(cb_unload_plug, NULL);
	aal_free(plugins);
}

/* Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information. */
errno_t reiser4_factory_foreach(plug_func_t plug_func, void *data) {
	errno_t res;
	uint8_t i;
	
	aal_assert("umka-3006", plug_func != NULL);

	for (i = 0; i < plugs_max[LAST_PLUG_TYPE]; i++) {
		if ((res = plug_func(plugins[i], data)))
			return res;
	}
	
	return 0;
}

/* Finds plugin by its type and id. */
reiser4_plug_t *reiser4_factory_ifind(rid_t type, rid_t id) {
	return plugins[plug_hash_func(type, id)];
}

/* Finds plugin by variable criterios implemented by passed @plug_func. */
reiser4_plug_t *reiser4_factory_cfind(plug_func_t plug_func, void *data) {
	errno_t res;
	uint8_t i;
	
	aal_assert("vpf-1886", plug_func != NULL);

	for (i = 0; i < plugs_max[LAST_PLUG_TYPE]; i++) {
		if ((res = plug_func(plugins[i], data)))
			return plugins[i];
	}
	
	return NULL;
}

#ifndef ENABLE_MINIMAL
/* Makes search for plugin by @name. */
reiser4_plug_t *reiser4_factory_nfind(char *name) {
	uint8_t i;
	
	for (i = 0; i < plugs_max[LAST_PLUG_TYPE]; i++) {
		if (!aal_strncmp(plugins[i]->label, name,
				 sizeof(plugins[i]->label)))
		{
			return plugins[i];
		}
	}
	
	return NULL;
}
#endif
