/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "sym40.h"

errno_t sym40_realize(object_info_t *info) {
	sdext_lw_hint_t lw_hint;
	errno_t res;

	aal_assert("vpf-1124", info != NULL);
	
	/* Symlink is kept in the SD, if SD was not found (and realized) nothing 
	   to looking for anymore. */
	if (!info->start.item.plugin)
		return -EINVAL;
	
	/* Double check that this is SD item. */
	aal_assert("vpf-1125", info->start.item.plugin->h.group == STATDATA_ITEM);
	
	/* This is a SD item. It must be the sym SD. */
	if ((res = obj40_read_lw(&info->start.item, &lw_hint)))
		return res;
	
	return S_ISLNK(lw_hint.mode) ? 0 : -EINVAL;
}

#endif

