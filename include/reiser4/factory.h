/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   factory.h -- plugin factory header file. */

#ifndef REISER4_FACTORY_H
#define REISER4_FACTORY_H

#include <reiser4/types.h>

extern void reiser4_factory_fini(void);
extern errno_t reiser4_factory_init(void);

extern errno_t reiser4_plug_fini(plug_class_t *class);
extern errno_t reiser4_factory_load(plug_desc_t *desc);
extern errno_t reiser4_factory_unload(reiser4_plug_t *plug);
extern reiser4_plug_t *reiser4_plug_init(plug_class_t *class);

#ifndef ENABLE_STAND_ALONE
extern reiser4_plug_t *reiser4_factory_nfind(char *name);

extern errno_t reiser4_factory_foreach(plug_func_t plug_func,
				       void *data);
#endif

extern reiser4_plug_t *reiser4_factory_ifind(rid_t type, rid_t id);

extern reiser4_plug_t *reiser4_factory_cfind(plug_func_t plug_func,
					     void *data);
#endif

