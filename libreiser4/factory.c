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
aal_list_t *plugins;
unsigned int registered = 0;

#if defined(ENABLE_STAND_ALONE) || defined(ENABLE_MONOLITHIC)

#ifndef ENABLE_STAND_ALONE
#define MAX_BUILTINS 50
#else
#define MAX_BUILTINS 16
#endif

/* Builtin plugin representative struct */
struct plug_builtin {
	plug_init_t init;
#ifndef ENABLE_STAND_ALONE
	plug_fini_t fini;
#endif
};

typedef struct plug_builtin plug_builtin_t;

/* The array of entry points of the all builtin plugins */
static plug_builtin_t __builtins[MAX_BUILTINS];
#endif

/* The struct which contains libreiser4 functions, they may be used by all
   plugin (tree_insert(), tree_remove(), etc). */
extern reiser4_core_t core;

/* Helper structure used in searching of plugins */
struct walk_desc {
	rid_t id;			    /* needed plugin id */
	rid_t type;		            /* needed plugin type */
	char *label;                        /* plugin label */
};

typedef struct walk_desc walk_desc_t;

#ifdef ENABLE_PLUGINS_CHECK
/* Helper callback for checking plugin validness */
static errno_t callback_check_plug(reiser4_plug_t *plug,
				   void *data)
{
	reiser4_plug_t *examined = (reiser4_plug_t *)data;

	if (examined == plug)
		return 0;

#ifndef ENABLE_STAND_ALONE
	/* Check plugins labels */
	if (!aal_strncmp(examined->label, plug->label,
			 PLUG_MAX_LABEL))
	{
		aal_exception_error("Plugin %s has the same label "
				    "as %s.", examined->cl.location,
				    plug->cl.location);
		return -EINVAL;
	}
#endif
	
	/* Check plugin group */
	if (examined->id.group >= LAST_ITEM) {
		aal_exception_error("Plugin %s has invalid group id "
				    "0x%x.", examined->cl.location,
				    examined->id.group);
		return -EINVAL;
	}

	/* Check plugin place */
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
/* Helper function for searching for the needed symbol inside loaded dynamic
   library. */
static void *find_symbol(void *handle, char *name, char *plug) {
	void *addr;
	
	if (!name || !handle)
		return NULL;
	
	/* Getting plugin entry point */
	addr = dlsym(handle, name);
	
	if (dlerror() != NULL || addr == NULL) {
		aal_exception_error("Can't find symbol %s in plugin "
				    "%s. %s.", name, plug, dlerror());
		return NULL;
	}
	
	return addr;
}

/* Loads plugin (*.so file) by its filename */
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

	/* Getting plugin init function */
	if (!(addr = find_symbol(class->data, "__plugin_init", (char *)name)))
		goto error_free_data;
    
	class->init = *((plug_init_t *)addr);

	/* Getting plugin fini function */
	if (!(addr = find_symbol(class->data, "__plugin_fini", (char *)name)))
		goto error_free_data;
    
	class->fini = *((plug_fini_t *)addr);
	return 0;
    
 error_free_data:
	dlclose(class->data);
	return -EINVAL;
}

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

	/* Checking plugin for validness (the same ids, etc) */
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

	/* Registering plugin in plugins list */
	plugins = aal_list_append(plugins, plug);

	return 0;

 error_free_plug:
	reiser4_factory_unload(plug);
	return res;
}

#else

/* Loads built-in plugin by its entry address */
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
	
	plugins = aal_list_append(plugins, plug);
	return 0;
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
	plugins = aal_list_remove(plugins, plug);

	return 0;
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
                                                                                                
        aal_assert("umka-159", plugins == NULL);
                                                                                            
#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
        if (!(dir = opendir(PLUGIN_DIR))) {
                aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK,
                                    "Can't open directory %s.",
				    PLUGIN_DIR);
                return -EINVAL;
        }
                                                                                                
        /* Getting plugins filenames */
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
			aal_free(name);
			continue;
		}
		
		aal_free(name);
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
		if (reiser4_factory_load(builtin->init, builtin->fini))
                        continue;
#else
		if (reiser4_factory_load(builtin->init, NULL))
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
void reiser4_factory_fini(void) {
	aal_list_t *walk, *next;

	aal_assert("umka-335", plugins != NULL);

	/* Unloading all registered plugins */
	for (walk = plugins; walk; walk = next) {
		next = walk->next;
		reiser4_factory_unload((reiser4_plug_t *)walk->data);
	}
	
	registered = 0;
	plugins = NULL;
}

/* Helper callback function for matching plugin by type and id */
static int callback_match_id(const void *plug,
			     const void *desc,
			     void *data)
{
	walk_desc_t *d = (walk_desc_t *)desc;
	reiser4_plug_t *p = (reiser4_plug_t *)plug;

	return !(p->id.type == d->type && p->id.id == d->id);
}

/* Finds plugins by its type and id */
reiser4_plug_t *reiser4_factory_ifind(
	rid_t type,			         /* requested plugin type */
	rid_t id)				 /* requested plugin id */
{
	aal_list_t *found;
	walk_desc_t desc;

	aal_assert("umka-155", plugins != NULL);    
	
	desc.id = id;
	desc.type = type;

	found = aal_list_find_custom(plugins, (void *)&desc,
				     callback_match_id, NULL);

	return found ? (reiser4_plug_t *)found->data : NULL;
}

/* Finds first matches plugin */
reiser4_plug_t *reiser4_factory_cfind(
	plug_func_t plug_func,                   /* per plugin function */
	void *data)                              /* user-specified data */
{
	aal_list_t *walk;

	aal_assert("umka-155", plugins != NULL);    
	aal_assert("umka-899", plug_func != NULL);    

	aal_list_foreach_forward(plugins, walk) {
		reiser4_plug_t *plug = (reiser4_plug_t *)walk->data;

		if (plug_func(plug, data))
			return plug;
	}

	return NULL;
}

#ifndef ENABLE_STAND_ALONE
/* Helper callback for matching plugin by its name */
static int callback_match_name(reiser4_plug_t *plug,
			       char *label, void *data)
{
	return aal_strncmp(plug->label, label,
			   sizeof(plug->label));
}

/* Makes search for plugin by name */
reiser4_plug_t *reiser4_factory_nfind(
	char *name)			        /* needed plugin name */
{
	aal_list_t *found;

	aal_assert("vpf-156", name != NULL);    
       
	found = aal_list_find_custom(aal_list_first(plugins), name,
				     (comp_func_t)callback_match_name,
				     NULL);

	return found ? (reiser4_plug_t *)found->data : NULL;
}
#endif

#if !defined(ENABLE_STAND_ALONE) || defined(ENABLE_PLUGINS_CHECK)
/* Calls specified function for every plugin from plugin list. This functions is
   used for getting any plugins information. */
errno_t reiser4_factory_foreach(
	plug_func_t plug_func,                   /* per plugin function */
	void *data)                              /* user-specified data */
{
	errno_t res = 0;
	aal_list_t *walk;
    
	aal_assert("umka-479", plug_func != NULL);

	aal_list_foreach_forward(plugins, walk) {
		reiser4_plug_t *plug = (reiser4_plug_t *)walk->data;
	
		if ((res = plug_func(plug, data)))
			return res;
	}
	
	return res;
}
#endif

#if defined(ENABLE_STAND_ALONE) || defined(ENABLE_MONOLITHIC)
/* This function registers builtin plugin entry points */
void register_builtin(plug_init_t init, plug_fini_t fini) {

	if (registered >= MAX_BUILTINS)
		registered = 0;
		
	__builtins[registered].init = init;
	
#ifndef ENABLE_STAND_ALONE
	__builtins[registered].fini = fini;
#endif

	registered++;
}

register_builtin_t __register_builtin = register_builtin;
#endif
