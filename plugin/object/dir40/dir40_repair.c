/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   dir40_repair.c -- reiser4 default directory file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "dir40.h"
#include "repair/plugin.h"

extern reiser4_plug_t dir40_plug;
extern lookup_t dir40_lookup(object_entity_t *entity, char *name, 
			     entry_hint_t *entry);

/* Check SD extentions and that mode in LW extention is REGFILE. */
static errno_t callback_sd(place_t *sd) {
	sdext_lw_hint_t lw_hint;
	uint64_t mask, extmask;
	errno_t res;
	
	/*  SD may contain LW and UNIX extentions only. 
	    FIXME-VITALY: tail policy is not supported yet. */
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);
	
	if ((extmask = obj40_extmask(sd)) == MAX_UINT64)
		return -EINVAL;
	
	if (mask != extmask)
		return RE_FATAL;
	
	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(sd, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	return S_ISDIR(lw_hint.mode) ? 0 : RE_FATAL;
}

/* Build the key of the '.'. */
static errno_t callback_body(object_info_t *info, key_entity_t *key) {
	uint64_t locality, objectid;
	
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
		
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);
	
	plug_call(info->object.plug->o.key_ops, build_entry, 
		  key, NULL, locality, objectid, ".");

	return 0;
}

object_entity_t *dir40_realize(object_info_t *info) {
	dir40_t *dir;
	errno_t res;
	
	if ((res = obj40_realize(info, callback_sd, callback_body,
				 1 << KEY_FILENAME_TYPE)))
		return res < 0 ? INVAL_PTR : NULL;
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plug, NULL, core, info->tree);
	
	return (object_entity_t *)dir;
}

errno_t dir40_check_attach(object_entity_t *object, object_entity_t *parent, 
			   uint8_t mode)
{
	dir40_t *odir = (dir40_t *)object;
	dir40_t *pdir = (dir40_t *)parent;
	entry_hint_t entry;
	lookup_t lookup;
	uint32_t links;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	aal_strncpy(entry.name, "..", 2);

	lookup = dir40_lookup(object, entry.name, &entry);

	switch(lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!plug_call(entry.object.plug->o.key_ops, compfull, 
			       &entry.object, STAT_KEY(&pdir->obj)))
			break;

		return RE_FATAL;
	case ABSENT:
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&odir->obj)->plug->o.key_ops, assign,
			  &entry.object, STAT_KEY(&pdir->obj));
		
		if ((res = plug_call(object->plug->o.object_ops,
				     add_entry, object, &entry)))
			return res;
	case FAILED:
		return -EINVAL;
	}

	/* ".." matches the parent. Now do parent->nlink++ for REBUILD mode. */
	return mode == RM_BUILD ? 
	       plug_call(parent->plug->o.object_ops, link, parent) :
	       RE_OK;
}

#endif

