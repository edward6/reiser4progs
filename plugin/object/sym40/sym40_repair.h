/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.h -- reiser4 symlink plugin repair functions. */

#ifndef SYM40_REPAIR_H
#define SYM40_REPAIR_H

#include "plugin/object/obj40/obj40_repair.h"

extern errno_t sym40_check_struct(reiser4_object_t *sym,
				  place_func_t place_func,
				  void *data, uint8_t mode);

extern errno_t sym40_recognize(reiser4_object_t *sym);
#endif

