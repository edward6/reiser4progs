/*
  sym40.h -- reiser4 symlink plugin structures.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef SYM40_H
#define SYM40_H

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#  include <limits.h>
#endif

#include <aal/aal.h>
#include <aux/aux.h>
#include <sys/stat.h>

#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

#ifdef ENABLE_STAND_ALONE
#  define _SYMLINK_LEN 256
#else
#  define _SYMLINK_LEN _POSIX_PATH_MAX
#endif

/* Compaund directory structure */
struct sym40 {

	/*
	  Common file fiedls (statdata, etc). As symlink40 has nothing but
	  statdata only, this structure has only file handler, which contains
	  stuff for statdata handling.
	*/
	obj40_t obj;
};

typedef struct sym40 sym40_t;
#endif

