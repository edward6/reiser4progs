/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.h -- reiser4 regular file plugin repair functions. */

#ifndef REG40_REPAIR_H
#define REG40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t reg40_form(object_entity_t *object);

extern errno_t reg40_check_struct(object_entity_t *object,
				  place_func_t place_func,
				  void *data, uint8_t mode);

extern object_entity_t *reg40_recognize(object_info_t *info);
#endif

