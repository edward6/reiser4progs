/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40.h -- reiser4 nodeptr item structures. */

#ifndef NODEPTR40_H
#define NODEPTR40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

typedef struct nodeptr40 {
	d64_t ptr;
} nodeptr40_t;

extern reiser4_core_t *nodeptr40_core;

#define nodeptr40_body(place) ((nodeptr40_t *)place->body)

#define np40_get_ptr(np)	aal_get_le64(np, ptr)
#define np40_set_ptr(np, val)	aal_set_le64(np, ptr, val)

#endif

