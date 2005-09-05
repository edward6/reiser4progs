/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40.h -- reiser4 regular file plugin structures. */

#ifndef REG40_H
#define REG40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>
#include <plugin/object/obj40/obj40.h>

#ifndef ENABLE_MINIMAL
extern int64_t reg40_put(reiser4_object_t *reg, void *buff, 
			 uint64_t n, place_func_t place_func);

extern errno_t reg40_seek(reiser4_object_t *reg,
			  uint64_t offset);

extern errno_t reg40_reset(reiser4_object_t *reg);
extern uint64_t reg40_offset(reiser4_object_t *reg);
extern lookup_t reg40_update_body(reiser4_object_t *reg);

#endif
#endif
