/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "dir40.h"

bool_t realize_mode_dir(uint16_t mode) {
	return S_ISDIR(mode);
}

errno_t dir40_realize(object_info_t *info) {
	return obj40_realize(info, realize_mode_dir, KEY_FILENAME_TYPE);
}

#endif

