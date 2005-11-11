/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.h -- plugin factory header file. */

#ifndef REISER4_FACTORY_H
#define REISER4_FACTORY_H

#include <reiser4/types.h>

extern void reiser4_factory_fini(void);
extern errno_t reiser4_factory_init(void);

#ifndef ENABLE_MINIMAL
extern reiser4_plug_t *reiser4_factory_nfind(char *name);
#endif

extern errno_t reiser4_factory_foreach(plug_func_t plug_func,
				       void *data);

extern void reiser4_factory_load(reiser4_plug_t *plug);

extern reiser4_plug_t *reiser4_factory_ifind(rid_t type, rid_t id);

extern reiser4_plug_t *reiser4_factory_cfind(plug_func_t plug_func,
					     void *data);
#endif

