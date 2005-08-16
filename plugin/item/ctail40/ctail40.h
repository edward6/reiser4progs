/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ctail40.h -- reiser4 compressed tail item plugin functions. */

#ifndef CTAIL40_H
#define CTAIL40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern reiser4_core_t *ctail40_core;

typedef struct ctail40 {
	/* Cluster size is block size shifted to this field. */
	d8_t shift;
	d8_t body[0];
} ctail40_t;

#define ct40_get_shift(ct)	(((ctail40_t *)ct)->shift)
#define ct40_set_shift(ct, val)	(((ctail40_t *)ct)->shift = val)

#endif
