/*
  factory.h -- plugin factory header file.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
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
errno_t libreiser4_factory_check(void);

extern errno_t libreiser4_factory_foreach(reiser4_plugin_func_t func, 
					  void *data);

extern reiser4_plugin_t *libreiser4_factory_ifind(rpid_t type, rpid_t id);
extern reiser4_plugin_t *libreiser4_factory_nfind(rpid_t type, const char *name);

extern reiser4_plugin_t *libreiser4_factory_cfind(reiser4_plugin_func_t func,
						  void *data);

extern reiser4_plugin_t *libreiser4_plugin_init(plugin_handle_t *handle);
errno_t libreiser4_plugin_fini(plugin_handle_t *handle);

#if !defined(ENABLE_COMPACT) && !defined(ENABLE_MONOLITHIC)

errno_t libreiser4_plugin_file_load(const char *name, plugin_handle_t *handle);
void libreiser4_plugin_file_uload(plugin_handle_t *handle);

#else

extern errno_t libreiser4_plugin_entry_load(unsigned long *entry, plugin_handle_t *handle);
extern void libreiser4_plugin_entry_uload(plugin_handle_t *handle);

#endif

#endif

