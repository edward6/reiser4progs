/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40.h -- reiser4 hashed directory plugin structures. */

#ifndef DIR40_H
#define DIR40_H

#include <aal/libaal.h>
#include "plugin/object/obj40/obj40.h"

#ifndef ENABLE_MINIMAL
extern errno_t dir40_reset(reiser4_object_t *dir);

extern lookup_t dir40_lookup(reiser4_object_t *dir,
			     char *name, entry_hint_t *entry);

extern errno_t dir40_fetch(reiser4_object_t *dir, 
			   entry_hint_t *entry);

extern errno_t dir40_entry_comp(reiser4_object_t *dir, void *data);

#endif
#endif
