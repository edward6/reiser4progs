/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 default regular file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "reg40.h"

bool_t realize_mode_reg(uint16_t mode) {
	return S_ISREG(mode);
}

errno_t reg40_realize(object_info_t *info) {
	return obj40_realize(info, realize_mode_reg, KEY_FILEBODY_TYPE);
}

#endif

