/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40.h -- reiser4 symlink plugin structures. */

#ifndef SYM40_H
#define SYM40_H

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#  include <limits.h>
#endif

#include <aal/libaal.h>
#include <aux/aux.h>
#include <sys/stat.h>

#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

#ifdef ENABLE_STAND_ALONE
#  define _SYMLINK_LEN 256
#else
#  define _SYMLINK_LEN _POSIX_PATH_MAX
#endif

/* Symlink struct. */
struct sym40 {

	/* Common file fiedls (statdata, etc). As sym40 has nothing but statdata
	   only, this structure has only file handler, which contains stuff for
	   statdata handling. */
	obj40_t obj;
};

typedef struct sym40 sym40_t;

extern reiser4_core_t *sym40_core;
extern reiser4_plug_t sym40_plug;

extern object_entity_t *sym40_open(object_info_t *info);
#endif

