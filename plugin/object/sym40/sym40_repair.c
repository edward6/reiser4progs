/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "sym40.h"

extern reiser4_plug_t sym40_plug;

extern object_entity_t *sym40_open(object_info_t *info);

object_entity_t *sym40_realize(object_info_t *info) {
	sym40_t *sym;
	sdext_lw_hint_t lw_hint;
	uint64_t mask, extmask;
	errno_t res;
	
	aal_assert("vpf-1124", info != NULL);
	
	/* Symlink is kept in the SD, if SD was not found (and realized) nothing 
	   to looking for anymore. */
	if (!info->start.plug)
		return NULL;
	
	/* Double check that this is SD item. */
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
	
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID | 1 << SDEXT_SYMLINK_ID);
	
	if ((extmask = obj40_extmask(&info->start)) == MAX_UINT64)
		return INVAL_PTR;
	
	if (extmask != mask)
		return INVAL_PTR;
	
	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(&info->start, SDEXT_LW_ID, &lw_hint)) < 0)
		return INVAL_PTR;
	
	if (!S_ISLNK(lw_hint.mode))
	    return INVAL_PTR;
	
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return INVAL_PTR;
	
	obj40_init(&sym->obj, &sym40_plug, core, info);

	return (object_entity_t *)sym;
}

#endif

