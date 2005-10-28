/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.h -- reiser4 regular file plugin repair functions. */

#ifndef REG40_REPAIR_H
#define REG40_REPAIR_H

#include <aal/libaal.h>
#include "reiser4/plugin.h"
#include "plugin/object/obj40/obj40.h"
#include "plugin/object/obj40/obj40_repair.h"

extern errno_t reg40_form(reiser4_object_t *object);

extern errno_t reg40_check_struct(reiser4_object_t *object,
				  place_func_t place_func,
				  void *data, uint8_t mode);

extern errno_t reg40_recognize(reiser4_object_t *reg);
#endif
