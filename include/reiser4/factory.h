/*
  factory.h -- plugin factory header file.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef REISER4_FACTORY_H
#define REISER4_FACTORY_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

extern void libreiser4_factory_fini(void);
extern errno_t libreiser4_factory_init(void);

#ifndef ENABLE_STAND_ALONE
extern reiser4_plugin_t *libreiser4_factory_nfind(const char *name);
#endif

#if !defined(ENABLE_STAND_ALONE) || defined(ENABLE_PLUGINS_CHECK)
extern errno_t libreiser4_factory_foreach(plugin_func_t plugin_func,
					  void *data);
#endif

extern reiser4_plugin_t *libreiser4_factory_ifind(rid_t type,
						  rid_t id);

extern void libreiser4_plugin_close(plugin_class_t *class);
extern errno_t libreiser4_plugin_fini(plugin_class_t *class);
extern errno_t libreiser4_factory_unload(reiser4_plugin_t *plugin);
extern reiser4_plugin_t *libreiser4_plugin_init(plugin_class_t *class);

#if !defined(ENABLE_STAND_ALONE) && !defined(ENABLE_MONOLITHIC)
extern errno_t libreiser4_factory_load(char *name);

errno_t libreiser4_plugin_open(const char *name,
			       plugin_class_t *class);

#else
extern errno_t libreiser4_plugin_open(plugin_init_t init,
				      plugin_fini_t fini,
				      plugin_class_t *class);

extern errno_t libreiser4_factory_load(plugin_init_t init,
				       plugin_fini_t fini);
#endif

extern reiser4_plugin_t *libreiser4_factory_cfind(plugin_func_t plugin_func,
						  void *data, bool_t only);

#endif

