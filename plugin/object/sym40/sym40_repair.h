/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.h -- reiser4 symlink plugin repair functions. */

#ifndef SYM40_REPAIR_H
#define SYM40_REPAIR_H

#include "sym40.h"
#include "plugin/object/obj40/obj40_repair.h"

extern errno_t sym40_check_struct(object_entity_t *object,
				  place_func_t place_func,
				  void *data, uint8_t mode);

extern object_entity_t *sym40_recognize(object_info_t *info);
#endif

