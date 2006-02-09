/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.c -- reiser4 plugin factory.It contains code for loading, unloading
   and finding plugins. */

#include <reiser4/libreiser4.h>

/* Hash table contains all known libreiser4 plugins. */
reiser4_plug_t **plugins;

static uint8_t plugs_max[LAST_PLUG_TYPE + 1] = {
	[OBJECT_PLUG_TYPE]	= OBJECT_LAST_ID,
	[ITEM_PLUG_TYPE]	= ITEM_LAST_ID,
	[NODE_PLUG_TYPE]	= NODE_LAST_ID,
	[HASH_PLUG_TYPE]	= HASH_LAST_ID,
	[FIBRE_PLUG_TYPE]	= FIBRE_LAST_ID,
	[POLICY_PLUG_TYPE]	= TAIL_LAST_ID,
	[SDEXT_PLUG_TYPE]	= SDEXT_LAST_ID,
	[FORMAT_PLUG_TYPE]	= FORMAT_LAST_ID,
	[OID_PLUG_TYPE]		= OID_LAST_ID,
	[ALLOC_PLUG_TYPE]	= ALLOC_LAST_ID,
	[JOURNAL_PLUG_TYPE]	= JOURNAL_LAST_ID,
	[KEY_PLUG_TYPE]		= KEY_LAST_ID,
	[COMPRESS_PLUG_TYPE]	= COMPRESS_LAST_ID,
	[CMODE_PLUG_TYPE]	= CMODE_LAST_ID,
	[CLUSTER_PLUG_TYPE]	= CLUSTER_LAST_ID,
	[LAST_PLUG_TYPE]	= 0,
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

/* Helper functions used for calculating hash and for comparing two entries from
   plugin hash table during its modifying. */
#define plug_hash_func(type, id) (plugs_max[type] + (id))
#define plug_type_count(type) ((uint8_t)(plugs_max[type + 1] - plugs_max[type]))

#ifndef ENABLE_MINIMAL
/* Unloads plugin and removes it from plugin hash table. */
static errno_t reiser4_factory_unload(reiser4_plug_t *plug) {
	aal_assert("umka-1496", plug != NULL);

	plugins[plug_hash_func(plug->id.type, plug->id.id)] = NULL;
	
	return 0;
}
#endif

/* Loads and initializes plugin by its entry. Also this function makes register
   the plugin in plugins list. */
void reiser4_factory_load(reiser4_plug_t *plug) {
#ifndef ENABLE_MINIMAL
	if (reiser4_factory_foreach(cb_check_plug, (void *)plug)) {
		aal_error("Plugin %s will not be attached to "
			  "plugin factory.", plug->label);
		reiser4_factory_unload(plug);
		return;
	}
#endif
	
	plugins[plug_hash_func(plug->id.type, plug->id.id)] = plug;
}

/* Macro for loading plugin by its name. */
#define __load_plug(name) {			\
	extern reiser4_plug_t name##_plug;	\
	reiser4_factory_load(&name##_plug);	\
}

#define __init_plug(name) {			\
	extern reiser4_core_t *name##_core;	\
	name##_core = &core;			\
}

/* Initializes all built-in plugins. Other kinds of plugins are not supported
   for now.  */
errno_t reiser4_factory_init(void) {
	uint8_t prev, max;
	int i;
	
	prev = 0;
	/* Init the plugin array. */
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
	__init_plug(format40);

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
	__init_plug(sdext_lw);
	
	__load_plug(sdext_lt);
	__init_plug(sdext_lt);
	
	__load_plug(sdext_unix);
	__init_plug(sdext_unix);
	
	__load_plug(sdext_pset);
	__init_plug(sdext_pset);
	
	__load_plug(sdext_hset);
#ifndef ENABLE_MINIMAL
	__load_plug(sdext_crypto);
	__init_plug(sdext_crypto);
#endif
	__load_plug(sdext_flags);
	__init_plug(sdext_flags);

	__load_plug(cde40);
	__init_plug(cde40);
	
	__load_plug(stat40);
	__init_plug(stat40);
	
	__load_plug(plain40);
	__init_plug(plain40);
#ifndef ENABLE_MINIMAL
	__load_plug(ctail40);
	__init_plug(ctail40);
#endif
	__load_plug(extent40);
	__init_plug(extent40);
	
	__load_plug(nodeptr40);
	__init_plug(nodeptr40);
	
	__load_plug(bbox40);
	__init_plug(bbox40);

#ifdef ENABLE_LARGE_KEYS
	__load_plug(key_large);
#endif
	
#ifdef ENABLE_SHORT_KEYS
	__load_plug(key_short);
#endif
	
	__load_plug(node40);
	__init_plug(node40);
	
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
	__init_plug(sdext_symlink);
	
	__load_plug(sym40);
#endif
	
	__init_plug(obj40);

#ifndef ENABLE_MINIMAL
	__load_plug(extents);
	__load_plug(smart);
	__load_plug(tails);

	__load_plug(lzo1);
	__load_plug(gzip1);

	__load_plug(nocompress);
	__load_plug(col8);
	__load_plug(col16);
	__load_plug(col32);
	__load_plug(coz);
	__load_plug(force);

	__load_plug(clust64);
	__load_plug(clust32);
	__load_plug(clust16);
	__load_plug(clust8);
	__load_plug(clust4);
#endif

        return 0;
}

/* Finalizes plugin factory, by means of unloading the all plugins. */
void reiser4_factory_fini(void) {
	aal_free(plugins);
}

/* Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information. */
errno_t reiser4_factory_foreach(plug_func_t plug_func, void *data) {
	errno_t res;
	uint8_t i;
	
	aal_assert("umka-3006", plug_func != NULL);

	for (i = 0; i < plugs_max[LAST_PLUG_TYPE]; i++) {
		if (plugins[i] && (res = plug_func(plugins[i], data)))
			return res;
	}
	
	return 0;
}

/* Finds plugin by its type and id. */
reiser4_plug_t *reiser4_factory_ifind(rid_t type, rid_t id) {
	if (type >= LAST_PLUG_TYPE)
		return 0;

	if (id >= plug_type_count(type))
		return 0;
	
	return plugins[plug_hash_func(type, id)];
}

/* Finds plugin by variable criterios implemented by passed @plug_func. */
reiser4_plug_t *reiser4_factory_cfind(plug_func_t plug_func, void *data) {
	errno_t res;
	uint8_t i;
	
	aal_assert("vpf-1886", plug_func != NULL);

	for (i = 0; i < plugs_max[LAST_PLUG_TYPE]; i++) {
		if (plugins[i] && (res = plug_func(plugins[i], data)))
			return plugins[i];
	}
	
	return NULL;
}

#ifndef ENABLE_MINIMAL
/* Makes search for plugin by @name. */
reiser4_plug_t *reiser4_factory_nfind(char *name) {
	uint8_t i;
	
	for (i = 0; i < plugs_max[LAST_PLUG_TYPE]; i++) {
		if (!plugins[i])
			continue;
		
		if (!aal_strncmp(plugins[i]->label, name,
				 sizeof(plugins[i]->label)))
		{
			return plugins[i];
		}
	}
	
	return NULL;
}
#endif
