/*
  nodeptr40.h -- reiser4 dafault internal item structures.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#ifndef NODEPTR40_H
#define NODEPTR40_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct nodeptr40 {
	d64_t ptr;
};

typedef struct nodeptr40 nodeptr40_t;

#define np40_get_ptr(np)	aal_get_le64(np, ptr)
#define np40_set_ptr(np, val)	aal_set_le64(np, ptr, val)

#endif

