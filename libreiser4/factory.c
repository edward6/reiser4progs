/*
  factory.c -- reiser4 plugin factory implementation.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
#  include <dlfcn.h>
#  include <dirent.h>
#  include <errno.h>
#  include <sys/types.h>
#endif

#include <reiser4/reiser4.h>

/* This list contain all known libreiser4 plugins */
static int last = 0;
aal_list_t *plugins;

#if defined(ENABLE_STAND_ALONE) || defined(ENABLE_MONOLITHIC)

#ifndef ENABLE_STAND_ALONE
#define MAX_BUILTINS 50
#else
#define MAX_BUILTINS 15
#endif

/* Builtin plugin representative struct */
struct plugin_builtin {
	plugin_init_t init;
#ifndef ENABLE_STAND_ALONE
	plugin_fini_t fini;
#endif
};

typedef struct plugin_builtin plugin_builtin_t;

/* The array of entry points of the all builtin plugins */
static plugin_builtin_t __builtins[MAX_BUILTINS];
#endif

/*
  The struct which contains libreiser4 functions, they may be used by all plugin
  (tree_insert(), tree_remove(), etc).
*/
extern reiser4_core_t core;

/* Helper structure used in searching of plugins */
struct walk_desc {
	rid_t id;			    /* needed plugin id */
	rid_t type;		            /* needed plugin type */
	const char *name;
};

typedef struct walk_desc walk_desc_t;

#ifdef ENABLE_PLUGINS_CHECK
/* Helper callback for checking plugin validness */
static errno_t callback_check_plugin(reiser4_plugin_t *plugin,
				     void *data)
{
	reiser4_plugin_t *examined = (reiser4_plugin_t *)data;

	if (examined == plugin)
		return 0;

	/* Check plugins labels */
	if (!aal_strncmp(examined->h.label, plugin->h.label,
			 PLUGIN_MAX_LABEL))
	{
		aal_exception_error("Plugin %s has the same label "
				    "as %s.", examined->h.class.name,
				    plugin->h.class.name);
		return -EINVAL;
	}

	/* Check plugin group */
	if (examined->h.group >= LAST_ITEM) {
		aal_exception_error("Plugin %s has invalid group id "
				    "0x%x.", examined->h.class.name,
				    examined->h.group);
		return -EINVAL;
	}

	/* Check plugin place */
	if (examined->h.group == plugin->h.group &&
	    examined->h.id == plugin->h.id &&
	    examined->h.type == plugin->h.type)
	{
		aal_exception_error("Plugin %s has the same sign as "
				    "%s.", examined->h.class.name,
				    plugin->h.class.name);
		return -EINVAL;
	}

	return 0;
}
#endif

#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)

/*
  Helper function for searcking for the needed symbol inside loaded dynamic
  library.
*/
static void *find_symbol(void *handle, char *name, char *plugin) {
	void *addr;
	
	if (!name || !handle)
		return NULL;
	
	/* Getting plugin entry point */
	addr = dlsym(handle, name);
	
	if (dlerror() != NULL || addr == NULL) {
		aal_exception_error("Can't find symbol %s in plugin "
				    "%s. %s.", name, plugin, dlerror());
		return NULL;
	}
	
	return addr;
}

/* Loads plugin (*.so file) by its filename */
errno_t libreiser4_plugin_open(const char *name,
			       plugin_class_t *class)
{
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

	/* Getting plugin init function */
	if (!(addr = find_symbol(class->data, "__plugin_init", (char *)name)))
		goto error_free_data;
    
	class->init = *((plugin_init_t *)addr);

	/* Getting plugin fini function */
	if (!(addr = find_symbol(class->data, "__plugin_fini", (char *)name)))
		goto error_free_data;
    
	class->fini = *((plugin_fini_t *)addr);
	return 0;
    
 error_free_data:
	dlclose(class->data);
	return -EINVAL;
}

void libreiser4_plugin_close(plugin_class_t *class) {
	void *data;
	
	aal_assert("umka-158", class != NULL);

	/*
	  Here we copy handle of the previously loaded library into address
	  space of the main process due to prevent us from the segfault after
	  plugin will be unloaded and we will unable to access memory area it
	  has just occupied.
	*/
	data = class->data;
	dlclose(data);
}

/*
  Loads plugin by is name (for instance, nodeptr40.so) and registers inside the
  plugin factory.
*/
errno_t libreiser4_factory_load(char *name) {
	errno_t res;

	plugin_class_t class;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1495", name != NULL);

	/* Open plugin and prepare its class */
	if ((res = libreiser4_plugin_open(name, &class)))
		return res;

	/*
	  Init plugin (in this point all plugin's global variables are
	  initializing too).
	*/
	if (!(plugin = class.init(&core)))
		return -EINVAL;

	/* Checking pluign for validness (the same ids, etc) */
	plugin->h.class = class;

#ifdef ENABLE_PLUGINS_CHECK
	if ((res = libreiser4_factory_foreach(callback_check_plugin,
					      (void *)plugin)))
	{
		aal_exception_warn("Plugin %s will not be attached to "
				   "plugin factory.", plugin->h.class.name);
		goto error_free_plugin;
	}
#endif

	/* Registering plugin in plugins list */
	plugins = aal_list_append(plugins, plugin);

	return 0;

 error_free_plugin:
	libreiser4_factory_unload(plugin);
	return res;
}

#else

/* Loads built-in plugin by its entry address */
errno_t libreiser4_plugin_open(plugin_init_t init,
			       plugin_fini_t fini,
			       plugin_class_t *class)
{

	aal_assert("umka-1431", init != NULL);
	aal_assert("umka-1432", class != NULL);

#ifndef ENABLE_STAND_ALONE
	aal_snprintf(class->name, sizeof(class->name),
		     "built-in (%p)", init);
#endif
	
	class->init = init;
	class->fini = fini;

	return 0;
}

/* Closes built-in plugins */
void libreiser4_plugin_close(plugin_class_t *class) {
	aal_assert("umka-1433", class != NULL);

	class->init = 0;
	class->fini = 0;
}

/*
  Loads and initializes plugin by its entry. Also this function makes register
  the plugin in plugins list.
*/
errno_t libreiser4_factory_load(plugin_init_t init,
				plugin_fini_t fini)
{
	errno_t res;

	plugin_class_t class;
	reiser4_plugin_t *plugin;
	
	if ((res = libreiser4_plugin_open(init, fini, &class)))
		return res;

	if (!(plugin = class.init(&core)))
		return -EINVAL;

	plugin->h.class = class;
	
#ifdef ENABLE_PLUGINS_CHECK
	if ((res = libreiser4_factory_foreach(callback_check_plugin,
					      (void *)plugin)))
	{
		aal_exception_warn("Plugin %s will not be attached to "
				   "plugin factory.", plugin->h.class.name);
		goto error_free_plugin;
	}
#endif
	
	plugins = aal_list_append(plugins, plugin);
	return 0;
	
 error_free_plugin:
	libreiser4_factory_unload(plugin);
	return res;
}

#endif

/* Finalizing the plugin */
errno_t libreiser4_factory_unload(reiser4_plugin_t *plugin) {
	plugin_class_t *class;
	
	aal_assert("umka-1496", plugin != NULL);
	
	class = &plugin->h.class;

	if (class->fini)
		class->fini(&core);
	
	libreiser4_plugin_close(class);
	plugins = aal_list_remove(plugins, plugin);

	return 0;
}

/* Initializes plugin factory by means of loading all available plugins */
errno_t libreiser4_factory_init(void) {
        reiser4_plugin_t *plugin;
                                                                                                
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
        DIR *dir;
        struct dirent *ent;
#else
	uint32_t i;
        plugin_builtin_t *builtin;
#endif
                                                                                                
        aal_assert("umka-159", plugins == NULL);
                                                                                                
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
        if (!(dir = opendir(PLUGIN_DIR))) {
                aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
                                    "Can't open directory %s.", PLUGIN_DIR);
                return -EINVAL;
        }
                                                                                                
        /* Getting plugins filenames */
        while ((ent = readdir(dir))) {
                char name[256];
                                                                                                
                if ((aal_strlen(ent->d_name) == 1 &&
		     aal_strncmp(ent->d_name, ".", 1)) ||
                    (aal_strlen(ent->d_name) == 2 &&
		     aal_strncmp(ent->d_name, "..", 2)))
                        continue;
                                                                                                
                if (aal_strlen(ent->d_name) <= 2)
                        continue;
                                                                                                
                if (ent->d_name[aal_strlen(ent->d_name) - 2] != 's' ||
                    ent->d_name[aal_strlen(ent->d_name) - 1] != 'o')
                        continue;
                                                                                                
                aal_memset(name, 0, sizeof(name));

                aal_snprintf(name, sizeof(name), "%s/%s",
			     PLUGIN_DIR, ent->d_name);
                                                                                                
                /* Loading plugin*/
                if (libreiser4_factory_load(name))
                        continue;
        }
                                                                                                
        closedir(dir);

        if (aal_list_len(plugins) == 0) {
                aal_exception_error("There are no valid plugins found "
                                    "in %s.", PLUGIN_DIR);
                return -EINVAL;
	}
#else
        /* Loads the all builtin plugins */
	for (i = 0; i < MAX_BUILTINS; i++) {
		builtin = &__builtins[i];

		if (!builtin->init)
			break;
#ifndef ENABLE_STAND_ALONE		
		if (libreiser4_factory_load(builtin->init, builtin->fini))
                        continue;
#else
		if (libreiser4_factory_load(builtin->init, NULL))
                        continue;
#endif
	}

#ifndef ENABLE_STAND_ALONE		
	if (aal_list_len(plugins) == 0) {
                aal_exception_error("There are no valid built-in plugins "
                                    "found.");
                return -EINVAL;
        }
#endif
	
#endif
        return 0;
}

/* Finalizes plugin factory, by means of unloading the all plugins */
void libreiser4_factory_fini(void) {
	aal_list_t *walk;
	plugin_class_t *class;

	aal_assert("umka-335", plugins != NULL);
    
	/* Unloading all registered plugins */
	for (walk = aal_list_last(plugins); walk; ) {
		aal_list_t *temp;
		reiser4_plugin_t *plugin;
		
		temp = walk->prev;
		plugin = (reiser4_plugin_t *)walk->data;
		
		libreiser4_factory_unload(plugin);
		walk = temp;
	}
	
	plugins = NULL;
}

/* Helper callback function for matching plugin by type and id */
static int callback_match_id(reiser4_plugin_t *plugin,
			     walk_desc_t *desc)
{
	return !(plugin->h.type == desc->type 
		&& plugin->h.id == desc->id);
}

/* Finds plugins by its type and id */
reiser4_plugin_t *libreiser4_factory_ifind(
	rid_t type,			         /* requested plugin type */
	rid_t id)				 /* requested plugin id */
{
	aal_list_t *found;
	walk_desc_t desc;

	aal_assert("umka-155", plugins != NULL);    
	
	desc.type = type;
	desc.id = id;

	found = aal_list_find_custom(aal_list_first(plugins), (void *)&desc, 
				     (comp_func_t)callback_match_id, NULL);

	return found ? (reiser4_plugin_t *)found->data : NULL;
}

/* Finds plugins by its type and id */
reiser4_plugin_t *libreiser4_factory_cfind(
	plugin_func_t plugin_func,               /* per plugin function */
	void *data)                              /* user-specified data */
{
	aal_list_t *walk = NULL;

	aal_assert("umka-155", plugins != NULL);    
	aal_assert("umka-899", plugin_func != NULL);    
	
	aal_list_foreach_forward(plugins, walk) {
		reiser4_plugin_t *plugin = (reiser4_plugin_t *)walk->data;

		if (plugin_func(plugin, data))
			return plugin;
	}
    
	return NULL;
}

#ifndef ENABLE_STAND_ALONE
/* Helper callback for matching plugin by its name */
static int callback_match_name(reiser4_plugin_t *plugin,
			       walk_desc_t *desc)
{
	return aal_strncmp(plugin->h.label, desc->name,
			   aal_strlen(desc->name));
}

/* Makes search for plugin by name */
reiser4_plugin_t *libreiser4_factory_nfind(
	const char *name)			 /* needed plugin name */
{
	aal_list_t *found;
	walk_desc_t desc;

	aal_assert("vpf-156", name != NULL);    
       
	desc.name = name;

	found = aal_list_find_custom(aal_list_first(plugins), (void *)&desc, 
				     (comp_func_t)callback_match_name, NULL);

	return found ? (reiser4_plugin_t *)found->data : NULL;
}
#endif

#if !defined(ENABLE_STAND_ALONE) || defined(ENABLE_PLUGINS_CHECK)
/* 
   Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information.
*/
errno_t libreiser4_factory_foreach(
	plugin_func_t plugin_func,               /* per plugin function */
	void *data)                              /* user-specified data */
{
	errno_t res = 0;
	aal_list_t *walk;
    
	aal_assert("umka-479", plugin_func != NULL);

	aal_list_foreach_forward(plugins, walk) {
		reiser4_plugin_t *plugin = (reiser4_plugin_t *)walk->data;
	
		if ((res = plugin_func(plugin, data)))
			return res;
	}
	
	return res;
}
#endif

#if defined(ENABLE_STAND_ALONE) || defined(ENABLE_MONOLITHIC)
/* This function registers builtin plugin entry points */
void register_builtin(plugin_init_t init, plugin_fini_t fini) {

	if (last >= MAX_BUILTINS)
		last = 0;
		
	__builtins[last].init = init;
	
#ifndef ENABLE_STAND_ALONE
	__builtins[last].fini = fini;
#endif

	last++;
}

register_builtin_t __register_builtin = register_builtin;
#endif
