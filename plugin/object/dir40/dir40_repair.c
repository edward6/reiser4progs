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

extern errno_t dir40_reset(object_entity_t *entity);
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

/* Set the key of "." taken from @info->start into @info->object */
static errno_t callback_key(obj40_t *obj) {
	entry_hint_t entry;
	
	if (obj->info.start.plug->o.item_ops->read == NULL)
		return -EINVAL;
	
	/* Read the first entry. */
	if (plug_call(obj->info.start.plug->o.item_ops, read, 
		      &obj->info.start, &entry, 0, 1) != 1)
		return -EINVAL;
	
	/* If not "." -- cannot obtain the "." key. */
	if (aal_strlen(entry.name) != 1 || aal_strncmp(entry.name, ".", 1))
		return RE_FATAL;
	
	aal_memcpy(&obj->info.object, &entry.object, sizeof(entry.object));
	
	return 0;
}

object_entity_t *dir40_realize(object_info_t *info) {
	dir40_t *dir;
	errno_t res;
	
	if (!(dir = aal_calloc(sizeof(*dir), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&dir->obj, &dir40_plug, core, info);
	
	if ((res = obj40_realize(&dir->obj, callback_sd, callback_key,
				 1 << KEY_FILENAME_TYPE)))
		goto error;
	
	/* Positioning to the first directory unit */
	if (dir40_reset((object_entity_t *)dir))
		goto error;
	
	return (object_entity_t *)dir;
 error:
	aal_free(dir);
	return res < 0 ? INVAL_PTR : NULL;
}

errno_t dir40_check_attach(object_entity_t *object, object_entity_t *parent, 
			   uint8_t mode)
{
	dir40_t *odir = (dir40_t *)object;
	key_entity_t *par_key;
	entry_hint_t entry;
	lookup_t lookup;
	uint32_t links;
	errno_t res;
	
	aal_assert("vpf-1151", object != NULL);
	aal_assert("vpf-1152", parent != NULL);
	
	aal_strncpy(entry.name, "..", 2);

	lookup = dir40_lookup(object, entry.name, &entry);

	if (!(par_key = plug_call(parent->plug->o.object_ops, origin,
				  parent)))
	{
		/* FIXME-UMKA->VITALY: Is it correct here? */
		return -EINVAL;
	}
	
	switch(lookup) {
	case PRESENT:
		/* If the key matches the parent -- ok. */
		if (!plug_call(entry.object.plug->o.key_ops, compfull, 
			       &entry.object, par_key))
			break;

		return RE_FATAL;
	case ABSENT:
		
		/* Adding ".." to the @object pointing to the @parent. */
		plug_call(STAT_KEY(&odir->obj)->plug->o.key_ops, assign,
			  &entry.object, par_key);
		
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

