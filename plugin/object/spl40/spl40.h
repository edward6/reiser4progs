/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40.h -- reiser4 special file plugin structures. */

#ifndef SPL40_H
#define SPL40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

/* Special file struct. */
struct spl40 {
	/* Common file fiedls (statdata, etc). As spl40 has nothing but statdata
	   only, this structure has only file handler, which contains stuff for
	   statdata handling. */
	obj40_t obj;
};

typedef struct spl40 spl40_t;

extern reiser4_plug_t spl40_plug;
extern reiser4_core_t *spl40_core;

#endif
