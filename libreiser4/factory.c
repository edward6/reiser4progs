/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.c -- reiser4 plugin factory implementation. */

#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
#  include <dlfcn.h>
#  include <errno.h>
#  include <dirent.h>
#  include <limits.h>
#  include <sys/types.h>
#endif

#include <reiser4/reiser4.h>

/* This list contain all known libreiser4 plugins */
uint32_t registered;
aal_hash_table_t *plugins;

#if defined(ENABLE_STAND_ALONE) || defined(ENABLE_MONOLITHIC)
/* Builtin plugin representative struct. */
struct plug_builtin {
	plug_init_t init;
#ifndef ENABLE_STAND_ALONE
	plug_fini_t fini;
#endif
};

typedef struct plug_builtin plug_builtin_t;

#ifndef ENABLE_STAND_ALONE
#  define MAX_BUILTIN_PLUGINS 50
#else
#  define MAX_BUILTIN_PLUGINS 16
#endif

/* The array of entry points of the all builtin plugins. */
static plug_builtin_t __builtins[MAX_BUILTIN_PLUGINS];
#endif

/* The struct which contains libreiser4 functions, they may be used by all
   plugin (tree_insert(), tree_remove(), etc). */
extern reiser4_core_t core;

#ifdef ENABLE_PLUGINS_CHECK
/* Helper callback for checking plugin validness. */
static errno_t callback_check_plug(reiser4_plug_t *plug,
				   void *data)
{
	reiser4_plug_t *examined = (reiser4_plug_t *)data;

	if (examined == plug)
		return 0;

#ifndef ENABLE_STAND_ALONE
	/* Check plugin labels. They should not be the same. */
	if (!aal_strncmp(examined->label, plug->label,
			 PLUG_MAX_LABEL))
	{
		aal_exception_error("Plugin %s has the same label "
				    "as %s.", examined->cl.location,
				    plug->cl.location);
		return -EINVAL;
	}
#endif
	
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

#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
/* Helper function for searching needed symbol inside loaded dynamic library. */
static void *find_symbol(void *handle, char *name, char *plug) {
	void *addr;
	
	if (!name || !handle)
		return NULL;
	
	/* Getting plugin entry point. */
	addr = dlsym(handle, name);
	
	if (dlerror() != NULL || addr == NULL) {
		aal_exception_error("Can't find symbol %s in plugin "
				    "%s. %s.", name, plug, dlerror());
		return NULL;
	}
	
	return addr;
}

/* Loads plugin (*.so file) by its filename. */
errno_t reiser4_plug_open(char *name, plug_class_t *class) {
	void *addr;
	
	aal_assert("umka-260", name != NULL);
	aal_assert("umka-1430", class != NULL);
    
	/* Loading specified plugin filename */
	if (!(class->data = dlopen(name, RTLD_NOW))) {
		aal_exception_error("Can't load plugin %s. %s.", 
				    name, dlerror());
		return errno;
	}

	aal_strncpy(class->name, name, sizeof(class->name));

	/* Getting plugin init function address. */
	if (!(addr = find_symbol(class->data, "__plugin_init", (char *)name)))
		goto error_free_data;
    
	class->init = *((plug_init_t *)addr);

	/* Getting plugin fini function address. */
	if (!(addr = find_symbol(class->data, "__plugin_fini", (char *)name)))
		goto error_free_data;
    
	class->fini = *((plug_fini_t *)addr);
	return 0;
    
 error_free_data:
	dlclose(class->data);
	return -EINVAL;
}

/* Closes libarry plugin handle stored in passed @class. */
void reiser4_plug_close(plug_class_t *class) {
	void *data;
	
	aal_assert("umka-158", class != NULL);

	/* Here we copy handle of the previously loaded library into varuable in
	   persistent address space of the process (not mapped due to loading
	   library) in order to prevent us from the segfault after plugin
	   library will be unloaded and we will unable to access memory area it
	   has just occupied. */
	data = class->data;
	dlclose(data);
}

/* Loads plugin by is name (for instance, nodeptr40.so) and registers inside the
   plugin factory. */
errno_t reiser4_factory_load(char *name) {
	errno_t res;

	plug_class_t class;
	reiser4_plug_t *plug;
	
	aal_assert("umka-1495", name != NULL);

	/* Open plugin and prepare its class */
	if ((res = reiser4_plug_open(name, &class)))
		return res;

	/* Init plugin (in this point all plugin's global variables are
	   initializing too). */
	if (!(plug = class.init(&core)))
		return -EINVAL;

	/* Checking plugin for validness (the ids, labels, etc) */
	plug->h.class = class;

#ifdef ENABLE_PLUGINS_CHECK
	if ((res = reiser4_factory_foreach(callback_check_plug,
					   (void *)plug)))
	{
		aal_exception_warn("Plugin %s will not be registereg in "
				   "plugin factory.", plug->h.class.name);
		goto error_free_plug;
	}
#endif

	/* Registering @plug in plugin factory hsh table. */
	return aal_hash_table_register(plugins, &plug->id, plug);

 error_free_plug:
	reiser4_factory_unload(plug);
	return res;
}

#else

/* Loads built-in plugin. */
errno_t reiser4_plug_open(plug_init_t init, plug_fini_t fini,
			  plug_class_t *class)
{

	aal_assert("umka-1431", init != NULL);
	aal_assert("umka-1432", class != NULL);

#ifndef ENABLE_STAND_ALONE
	aal_snprintf(class->location, sizeof(class->location),
		     "built-in (%p)", init);

	class->fini = fini;
#endif
	
	class->init = init;
	return 0;
}

/* Closes built-in plugins */
void reiser4_plug_close(plug_class_t *class) {
	aal_assert("umka-1433", class != NULL);

	class->init = 0;
#ifndef ENABLE_STAND_ALONE
	class->fini = 0;
#endif
}

/* Loads and initializes plugin by its entry. Also this function makes register
   the plugin in plugins list. */
errno_t reiser4_factory_load(plug_init_t init, plug_fini_t fini) {
	errno_t res;

	plug_class_t class;
	reiser4_plug_t *plug;
	
	if ((res = reiser4_plug_open(init, fini, &class)))
		return res;

	if (!(plug = class.init(&core)))
		return -EINVAL;

	plug->cl = class;
	
#ifdef ENABLE_PLUGINS_CHECK
	if ((res = reiser4_factory_foreach(callback_check_plug,
					   (void *)plug)))
	{
		aal_exception_warn("Plugin %s will not be attached to "
				   "plugin factory.", plug->cl.location);
		goto error_free_plug;
	}
#endif
	
	return aal_hash_table_insert(plugins, &plug->id, plug);

#ifdef ENABLE_PLUGINS_CHECK
 error_free_plug:
	reiser4_factory_unload(plug);
	return res;
#endif
}

#endif

/* Finalizing the plugin */
errno_t reiser4_factory_unload(reiser4_plug_t *plug) {
	plug_class_t *class;
	
	aal_assert("umka-1496", plug != NULL);
	
	class = &plug->cl;

#ifndef ENABLE_STAND_ALONE
	if (class->fini)
		class->fini(&core);
#endif
	
	reiser4_plug_close(class);
	aal_hash_table_remove(plugins, &plug->id);

	return 0;
}

static uint64_t callback_plugins_hash_func(void *key) {
	return (uint64_t)((plug_ident_t *)key)->type;
}

static int callback_plugins_comp_func(void *key1, void *key2,
				      void *data)
{
	return (!ident_equal((plug_ident_t *)key1,
			     (plug_ident_t *)key2));
}

/* Initializes plugin factory by means of loading all available plugins */
errno_t reiser4_factory_init(void) {
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
        DIR *dir;
        struct dirent *ent;
#else
	uint32_t i;
        plug_builtin_t *builtin;
#endif

	if (!(plugins = aal_hash_table_alloc(callback_plugins_hash_func,
					     callback_plugins_comp_func,
					     NULL, NULL)))
	{
		return -ENOMEM;
	}
	
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
        if (!(dir = opendir(PLUGIN_DIR))) {
                aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
                                    "Can't open directory %s.",
				    PLUGIN_DIR);
                return -EINVAL;
        }
                                                                                                
        /* Getting plugins filenames. */
        while ((ent = readdir(dir))) {
		uint32_t len;
                char *name;

                if ((len = aal_strlen(ent->d_name)) <= 3)
                        continue;
                                                                                                
                if (ent->d_name[len - 3] != '.' ||
		    ent->d_name[len - 2] != 's' ||
                    ent->d_name[len - 1] != 'o')
		{
                        continue;
		}

		if (!(name = aal_calloc(_POSIX_PATH_MAX + 1, 0)))
			return -ENOMEM;

                aal_snprintf(name, _POSIX_PATH_MAX, "%s/%s",
			     PLUGIN_DIR, ent->d_name);
                                                                                                
                /* Loading plugin at @name */
                if (reiser4_factory_load(name)) {
			aal_exception_warn("Can't load %s. Is it "
					   "reiser4 plugin at all?",
					   name);
			aal_free(name);
			continue;
		}
		
		aal_free(name);
        }
                                                                                                
        closedir(dir);

        if (plugins->real == 0) {
                aal_exception_error("There are no valid plugins found "
                                    "in %s.", PLUGIN_DIR);
                return -EINVAL;
	}
#else
        /* Loads the all builtin plugins. */
	for (i = 0; i < MAX_BUILTIN_PLUGINS; i++) {
		builtin = &__builtins[i];

		if (!builtin->init)
			break;
		
#ifndef ENABLE_STAND_ALONE		
		if (reiser4_factory_load(builtin->init, builtin->fini))
                        continue;
#else
		if (reiser4_factory_load(builtin->init, NULL)) {
			aal_exception_warn("Can't load built-in plugin."
					   "Init function address is 0x%x");
			continue;
		}
#endif
	}

#ifndef ENABLE_STAND_ALONE
	if (plugins->real == 0) {
                aal_exception_error("There are no valid built-in "
				    "plugins found.");
                return -EINVAL;
        }
#endif
	
#endif
        return 0;
}

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
	
	registered = 0;
}


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

struct find_hint {
	void *data;
	reiser4_plug_t *plug;
	plug_func_t plug_func;
};

typedef struct find_hint find_hint_t;

static errno_t callback_foreach_plug(void *entry, void *data) {
	errno_t res;

	find_hint_t *hint;
	reiser4_plug_t *plug;
	aal_hash_node_t *node;

	hint = (find_hint_t *)data;
	node = (aal_hash_node_t *)entry;
	plug = (reiser4_plug_t *)node->value;

	if ((res = hint->plug_func(plug, hint->data))) {
		hint->plug = plug;
		return res;
	}

	return 0;
}

#if !defined(ENABLE_STAND_ALONE) || defined(ENABLE_PLUGINS_CHECK)
/* Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information. */
errno_t reiser4_factory_foreach(plug_func_t plug_func, void *data) {
	find_hint_t hint;
	
	aal_assert("umka-3005", plugins != NULL);    
	aal_assert("umka-3006", plug_func != NULL);    

	hint.plug = NULL;
	hint.data = data;
	hint.plug_func = plug_func;

	return aal_hash_table_foreach(plugins, callback_foreach_plug, &hint);
}
#endif

/* Finds plugin by variable criterios implemented by passed @plug_func. */
reiser4_plug_t *reiser4_factory_cfind(plug_func_t plug_func, void *data) {
	find_hint_t hint;
	
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
static errno_t callback_nfind_plug(void *entry, void *data) {
	find_hint_t *hint = (find_hint_t *)data;
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

/* Makes search for plugin by name */
reiser4_plug_t *reiser4_factory_nfind(char *name) {
	find_hint_t hint;

	aal_assert("vpf-156", name != NULL);    
       
	hint.plug = NULL;
	hint.data = name;

	aal_hash_table_foreach(plugins, callback_nfind_plug,
			       &hint);
	
	return hint.plug;
}
#endif

#if defined(ENABLE_STAND_ALONE) || defined(ENABLE_MONOLITHIC)
/* This function registers builtin plugin entry points. */
void register_builtin(plug_init_t init, plug_fini_t fini) {

	if (registered >= MAX_BUILTIN_PLUGINS)
		registered = 0;
		
	__builtins[registered].init = init;
	
#ifndef ENABLE_STAND_ALONE
	__builtins[registered].fini = fini;
#endif

	registered++;
}

register_builtin_t __register_builtin = register_builtin;
#endif
