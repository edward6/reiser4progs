/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "dir40.h"

static errno_t callback_mode_dir(uint16_t mode) {
	return S_ISDIR(mode) ? 0 : -EINVAL;
}

static errno_t callback_type_name(uint16_t type) {
	return type == KEY_FILENAME_TYPE ? 0 : -EINVAL;
}

errno_t dir40_realize(object_info_t *info) {
	return obj40_realize(info, callback_mode_dir, callback_type_name);
}

#endif

