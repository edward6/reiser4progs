/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   blackbox40_repair.h -- reiser4 default safe link plugin repair method 
   declarations. */

#ifndef BLACKBOX40_REPAIR_H
#define BLACKBOX40_REPAIR_H
#include <reiser4/plugin.h>

extern reiser4_core_t *blackbox40_core;

extern errno_t blackbox40_check_struct(reiser4_place_t *place, uint8_t mode);

#endif
