/*
  factory.h -- plugin factory header file.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef FACTORY_H
#define FACTORY_H

#ifdef CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

extern errno_t libreiser4_factory_init(void);
extern void libreiser4_factory_done(void);

extern errno_t libreiser4_factory_foreach(reiser4_plugin_func_t func, 
					  void *data);

extern reiser4_plugin_t *libreiser4_factory_ifind(rpid_t type, rpid_t id);
extern reiser4_plugin_t *libreiser4_factory_nfind(rpid_t type, const char *name);

extern reiser4_plugin_t *libreiser4_factory_cfind(reiser4_plugin_func_t func,
						  void *data);

extern reiser4_plugin_t *libreiser4_plugin_init(plugin_handle_t *handle);
extern errno_t libreiser4_plugin_fini(plugin_handle_t *handle);

extern errno_t libreiser4_factory_unload(reiser4_plugin_t *plugin);

#if !defined(ENABLE_COMPACT) && !defined(ENABLE_MONOLITHIC)

extern errno_t libreiser4_factory_load(char *name);
extern void libreiser4_plugin_close(plugin_handle_t *handle);
errno_t libreiser4_plugin_open(const char *name, plugin_handle_t *handle);

#else

extern void libreiser4_plugin_close(plugin_handle_t *handle);
extern errno_t libreiser4_factory_load(unsigned long *entry);
extern errno_t libreiser4_plugin_open(unsigned long *entry, plugin_handle_t *handle);

#endif

#endif

