/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "sym40.h"

extern object_entity_t *sym40_open(object_info_t *info);

object_entity_t *sym40_realize(object_info_t *info) {
	sdext_lw_hint_t lw_hint;
	
	aal_assert("vpf-1124", info != NULL);
	
	/* Symlink is kept in the SD, if SD was not found (and realized) nothing 
	   to looking for anymore. */
	if (!info->start.plug)
		return NULL;
	
	/* Double check that this is SD item. */
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
	
	/* This is a SD item. It must be the sym SD. */
	if (obj40_read_lw(&info->start, &lw_hint))
		return NULL;
	
	if (!S_ISLNK(lw_hint.mode))
		return NULL;
	
	return sym40_open(info);
}

#endif

