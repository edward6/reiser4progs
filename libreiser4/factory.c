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

/* Helper structure used in searching of plugins */
struct walk_desc {
	rid_t id;			    /* needed plugin id */
	rid_t type;		            /* needed plugin type */
	const char *name;
};

typedef struct walk_desc walk_desc_t;

/* This list contain all known libreiser4 plugins */
aal_list_t *plugins = NULL;

extern reiser4_core_t core;

/* Helper callback function for matching plugin by type and id */
static int callback_match_id(
	reiser4_plugin_t *plugin,	         /* current plugin in list */
	walk_desc_t *desc)		         /* key to searched */
{
	return !(plugin->h.type == desc->type 
		&& plugin->h.id == desc->id);
}

/* Helper callback for matching plugin by its name */
static int callback_match_name(reiser4_plugin_t *plugin, walk_desc_t *desc) {
	return aal_strncmp(plugin->h.label, desc->name,
			   aal_strlen(desc->name));
}

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
				    "as %s.", examined->h.handle.name,
				    plugin->h.handle.name);
		return -EINVAL;
	}

	/* Check plugin group */
	if (examined->h.group >= LAST_ITEM) {
		aal_exception_error("Plugin %s has invalid group id "
				    "0x%x.", examined->h.handle.name,
				    examined->h.group);
		return -EINVAL;
	}

	/* Check plugin place */
	if (examined->h.group == plugin->h.group &&
	    examined->h.id == plugin->h.id &&
	    examined->h.type == plugin->h.type)
	{
		aal_exception_error("Plugin %s has the same sign as "
				    "%s.", examined->h.handle.name,
				    plugin->h.handle.name);
		return -EINVAL;
	}

	return 0;
}

#endif

/* Initializes plugin (that is calls its init method) by its handle */
reiser4_plugin_t *libreiser4_plugin_init(plugin_handle_t *handle) {
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-259", handle != NULL);
	aal_assert("umka-1429", handle->init != NULL);
    
	if (!(plugin = handle->init(&core))) {
		aal_exception_error("Can't initialiaze plugin %s.",
				    handle->name);
		return NULL;
	}
    
	return plugin;
}

/* Finalizes plugin by means of calling its fini method */
errno_t libreiser4_plugin_fini(plugin_handle_t *handle) {
	errno_t ret = 0;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1428", handle != NULL);
    
	if (handle->fini && (ret = handle->fini(&core))) {
		aal_exception_warn("Plugin %s finished with error %d.",
				   handle->name, ret);
	}
    
	return ret;
}

#ifndef ENABLE_STAND_ALONE
extern reiser4_abort_t abort_func;
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
		aal_exception_error("Can't find symbol %s in plugin %s. %s.", 
				    name, plugin, dlerror());
		return NULL;
	}
	
	return addr;
}

/* Loads plugin (*.so file) by its filename */
errno_t libreiser4_plugin_open(const char *name,
			       plugin_handle_t *handle)
{
	void *addr;
	
	aal_assert("umka-260", name != NULL);
	aal_assert("umka-1430", handle != NULL);
    
	aal_memset(handle, 0, sizeof(*handle));
	
	/* Loading specified plugin filename */
	if (!(handle->data = dlopen(name, RTLD_NOW))) {
		aal_exception_error("Can't load plugin %s. %s.", 
				    name, dlerror());
		return errno;
	}

	aal_strncpy(handle->name, name, sizeof(handle->name));

	/* Getting plugin init function */
	if (!(addr = find_symbol(handle->data, "__plugin_init", (char *)name)))
		goto error_free_handle;
    
	handle->init = *((reiser4_plugin_init_t *)addr);

	/* Getting plugin fini function */
	if (!(addr = find_symbol(handle->data, "__plugin_fini", (char *)name)))
		goto error_free_handle;
    
	handle->fini = *((reiser4_plugin_fini_t *)addr);

#ifndef ENABLE_STAND_ALONE
	handle->abort = abort_func;
#endif

	return 0;
    
 error_free_handle:
	dlclose(handle->data);
 error:
	return -1;
}

void libreiser4_plugin_close(plugin_handle_t *handle) {
	plugin_handle_t local;
	
	aal_assert("umka-158", handle != NULL);

	/*
	  Here we copy handle of the previously loaded library into address
	  space of the main process due to prevent us from the segfault after
	  plugin will be uploaded and we will unable access memory area it
	  occupied.
	*/
	local = *handle;
	dlclose(local.data);
}

/*
  Loads plugin by is name (for instance, nodeptr40.so) and registers inside the
  plugin factory.
*/
errno_t libreiser4_factory_load(char *name) {
	errno_t res;

	plugin_handle_t handle;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1495", name != NULL);

	/* Open plugin and prepare its handle */
	if ((res = libreiser4_plugin_open(name, &handle)))
		return res;

	/*
	  Init plugin (in this point all plugin's global variables are
	  initializing too).
	*/
	if (!(plugin = libreiser4_plugin_init(&handle)))
		return -EINVAL;

	/* Checking pluign for validness (the same ids, etc) */
	plugin->h.handle = handle;

#ifdef ENABLE_PLUGINS_CHECK
	if ((res = libreiser4_factory_foreach(callback_check_plugin,
					      (void *)plugin)))
	{
		aal_exception_warn("Plugin %s will not be attached to "
				   "plugin factory.", plugin->h.handle.name);
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
errno_t libreiser4_plugin_open(unsigned long *entry,
			       plugin_handle_t *handle)
{

	aal_assert("umka-1431", entry != NULL);
	aal_assert("umka-1432", handle != NULL);

	aal_memset(handle, 0, sizeof(*handle));
	
	aal_snprintf(handle->name, sizeof(handle->name),
		     "built-in (0x%lx)", *entry);
	
	handle->init = (reiser4_plugin_init_t)*entry;
	handle->fini = (reiser4_plugin_fini_t)*(entry + 1);

#ifndef ENABLE_STAND_ALONE
	handle->abort = abort_func;
#endif

	return 0;
}

/* Closes built-in plugins */
void libreiser4_plugin_close(plugin_handle_t *handle) {
	aal_assert("umka-1433", handle != NULL);
	aal_memset(handle, 0, sizeof(*handle));
}

/*
  Loads and initializes plugin by its entry. Also this function makes register
  the plugin in plugins list.
*/
errno_t libreiser4_factory_load(unsigned long *entry) {
	errno_t res;

	plugin_handle_t handle;
	reiser4_plugin_t *plugin;
	
	aal_assert("umka-1497", entry != NULL);
	
	if ((res = libreiser4_plugin_open(entry, &handle)))
		return res;

	if (!(plugin = libreiser4_plugin_init(&handle)))
		return -EINVAL;

	plugin->h.handle = handle;
	
#ifdef ENABLE_PLUGINS_CHECK
	if ((res = libreiser4_factory_foreach(callback_check_plugin,
					      (void *)plugin)))
	{
		aal_exception_warn("Plugin %s will not be attached to "
				   "plugin factory.", plugin->h.handle.name);
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
	plugin_handle_t *handle;
	
	aal_assert("umka-1496", plugin != NULL);
	
	handle = &plugin->h.handle;
	libreiser4_plugin_fini(handle);

	libreiser4_plugin_close(handle);
	plugins = aal_list_remove(plugins, plugin);

	return 0;
}

/* Initializes plugin factory by means of loading all available plugins */
errno_t libreiser4_factory_init(void) {
	plugin_handle_t handle;
	reiser4_plugin_t *plugin;
	
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
	DIR *dir;
	struct dirent *ent;
#else
	unsigned long *entry;
	extern unsigned long __plugin_start;
	extern unsigned long __plugin_end;
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

		if ((aal_strlen(ent->d_name) == 1 && aal_strncmp(ent->d_name, ".", 1)) ||
		    (aal_strlen(ent->d_name) == 2 && aal_strncmp(ent->d_name, "..", 2)))
			continue;	
	
		if (aal_strlen(ent->d_name) <= 2)
			continue;
		
		if (ent->d_name[aal_strlen(ent->d_name) - 2] != 's' || 
		    ent->d_name[aal_strlen(ent->d_name) - 1] != 'o')
			continue;
		
		aal_memset(name, 0, sizeof(name));
		aal_snprintf(name, sizeof(name), "%s/%s", PLUGIN_DIR, ent->d_name);

		/* Loading plugin*/
		if (libreiser4_factory_load(name))
			continue;
	}
	
	closedir(dir);
#else
	/* Loads the all built-in plugins */
	for (entry = &__plugin_start; entry < &__plugin_end; entry += 2) {

#ifndef ENABLE_STAND_ALONE
		if (!entry) {
			aal_exception_warn("Invalid built-in entry detected at "
					   "address (0x%lx).", &entry);
			continue;
		}
#endif

		if (libreiser4_factory_load(entry))
			continue;
	}
#endif
	if (aal_list_length(plugins) == 0) {
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
		aal_exception_error("There are no valid plugins found "
				    "in %s.", PLUGIN_DIR);
#else
		aal_exception_error("There are no valid built-in plugins "
				    "found.");
#endif
		return -EINVAL;
	}
	
	return 0;
}

/* Finalizes plugin factory, by means of unloading the all plugins */
void libreiser4_factory_fini(void) {
	aal_list_t *walk;
	plugin_handle_t *handle;

	aal_assert("umka-335", plugins != NULL);
    
	/* Unloading all registered plugins */
	for (walk = aal_list_last(plugins); walk; ) {
		aal_list_t *temp = aal_list_prev(walk);
		reiser4_plugin_t *plugin = (reiser4_plugin_t *)walk->data;
		
		libreiser4_factory_unload(plugin);
		walk = temp;
	}
	
	plugins = NULL;
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

/* Finds plugins by its type and id */
reiser4_plugin_t *libreiser4_factory_cfind(
	reiser4_plugin_func_t plugin_func,	 /* per plugin function */
	void *data)				 /* user-specified data */
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

/* 
   Calls specified function for every plugin from plugin list. This functions
   is used for getting any plugins information.
*/
errno_t libreiser4_factory_foreach(
	reiser4_plugin_func_t plugin_func,	/* per plugin function */
	void *data)			        /* user-specified data */
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
