/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   nodeptr40_repair.h -- reiser4 nodeptr repair functions. */

#ifndef NODEPTR40_REPAIR_H
#define NODEPTR40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t nodeptr40_check_struct(place_t *place,
				      uint8_t mode);

extern errno_t nodeptr40_check_layout(place_t *place,
				      region_func_t func, 
				      void *data, uint8_t mode);
#endif
