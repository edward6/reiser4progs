/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 default regular file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "reg40.h"
#include "repair/plugin.h"

extern reiser4_plug_t reg40_plug;

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
	
	return S_ISREG(lw_hint.mode) ? 0 : RE_FATAL;
}

static errno_t reg40_check_mode(place_t *sd, uint8_t mode) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(sd, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	if S_ISREG(lw_hint.mode) 
		return 0;
	
	aal_exception_error("Node (%llu), item (%u): statdata has wrong mode "
			    "(%o). Plugin (%s)", sd->con.blk, sd->pos.item,
			    lw_hint.mode, sd->plug->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	lw_hint.mode &= ~S_IFMT;
        lw_hint.mode |= S_IFREG;
	
	aal_exception_error("Node (%llu), item (%u): statdata mode is fixed "
			    "to (%o). Plugin (%s)", sd->con.blk, sd->pos.item,
			    lw_hint.mode, sd->plug->label);
	
	return obj40_write_ext(sd, SDEXT_LW_ID, &lw_hint);
}

/* Build the key of the reg40 object body. */
static errno_t callback_body(object_info_t *info, key_entity_t *key) {
	uint64_t type, locality, objectid, ordering;
	
	type = plug_call(info->object.plug->o.key_ops,
			 get_type, &info->object);
	
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
		
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);
	
	ordering = plug_call(info->object.plug->o.key_ops,
			     get_ordering, &info->object);
	
	plug_call(info->object.plug->o.key_ops, build_gener, 
		  key, type, locality, ordering, objectid, 0);
	
	return 0;
}

object_entity_t *reg40_realize(object_info_t *info) {
	reg40_t *reg;
	errno_t res;
	
	if ((res = obj40_realize(info, callback_sd, callback_body,
				 1 << KEY_FILEBODY_TYPE)))
		return res < 0 ? INVAL_PTR : NULL;
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, NULL, core, info->tree);
	
	return (object_entity_t *)reg;
}

errno_t reg40_check_sd(place_t *sd, uint8_t mode) {
#if 0
	errno_t res;
	
	if (mode == RM_BUILD) {
		/* Check that SD is of this file. Relocate if not. */
		if ((res = reg40_realize_sd(sd, RM_REALIZE)) < 0)
			return res;
		
		if (res & RE_FATAL) {
			aal_exception_fatal("Relocation is not ready yet.");
			return res;
		}

		/* SD seems to be of this plugin. Register. */
		res = register_func(object, &info->start, data);

		if (res < 0)
			return res;
		else if (res) {
			/* Relocate. */
			aal_exception_fatal("Relocation is not "
					    "ready yet.")
				return res;
		}

		/* Fix SD if needed. */
		if ((res = reg40_check_sd(sd, RM_REALIZE)) < 0)
			return res;

		if ((res & RE_FIXABLE) && 
		    reg40_check_sd(sd, mode))
		{
			aal_exception_fatal("Check SD must be "
					    "successful.");
			return -EINVAL;
		}
	} else {
		res = reg40_check_sd(sd, mode)
	}

	if ((res = reg40_check_sd(sd, RM_REALIZE)) < 0)
		return res;

	if (res & RE_FATAL) 
		/* Fix SD. */
		if ((res = obj40_read_ext(&info->start,
					  SDEXT_UNIX_ID,
					  &lw_hint)))
			return res;

	aal_assert("vpf-1128: Relocation is not ready "
		   "yet.", callback_mode(lw_hint.mode));
#endif
	return 0;
}

#if 0
/* Check that SD may contain LW and UNIX extentions only. Fix the mode 
   if wrong. FIXME-VITALY: tail policy is not supported yet as PLUG_ID 
   extention, is not ready. */
static errno_t reg40_realize_sd(place_t *sd, uint8_t mode) {
	sdext_lw_hint_t lw_hint;
	uint64_t mask, extmask;
	
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);
	
	if ((extmask = obj40_extmask(sd)) == MAX_UINT64)
		return -EINVAL;
	
	/* FIXME-VITALY: Check PLUG extention here also. */
	if (extmask != mask) {
		if (mode != RM_REALIZE) {
			aal_exception_error("Node (%llu), item (%u): statdata "
					    "has unknown set of extentions "
					    "(0x%llx), should be (0x%llx). "
					    "Plugin (%s)", sd->con.blk, 
					    sd->pos.item, stat.extmask,
					    mask, sd->plug->label);
		}
		
		return RE_FATAL;
	}
		
	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(sd, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	if (S_ISREG(lw_hint.mode))
		return 0;

	/* Mode is wrong, fix it if not CHECK mode. */
	if (mode == RM_REALIZE)
		return RE_FATAL;
	
	aal_exception_error("Node (%llu), item (%u): statdata has wrong mode "
			    "(%o). Plugin (%s)", sd->con.blk, sd->pos.item,
			    lw_hint.mode, sd->plug->label);
	
	if (mode == RM_CHECK)
		return RE_FIXABLE;
	
	lw_hint.mode &= ~S_IFMT;
        lw_hint.mode |= S_IFREG;
	
	aal_exception_error("Node (%llu), item (%u): statdata mode is fixed "
			    "to (%o). Plugin (%s)", sd->con.blk, sd->pos.item,
			    lw_hint.mode, sd->plug->label);
	
	return obj40_write_ext(sd, SDEXT_LW_ID, &lw_ext);
}
#endif

errno_t reg40_check_struct(object_entity_t *object, object_info_t *info,
			   place_func_t register_func, uint8_t mode,
			   void *data)
{
	uint64_t locality, objectid, ordering;
	reiser4_plug_t *kplug;
	errno_t res = RE_OK;
	key_entity_t key;
	lookup_t lookup;
	reg40_t *reg;

	aal_assert("vpf-1126", info != NULL);
	aal_assert("vpf-1190", info->tree != NULL);
	
	reg = (reg40_t *)object;
	
	/* Recovery on the base of an item. */
	if (info->start.plug) {
		kplug = info->start.key.plug;
		
		locality = plug_call(kplug->o.key_ops, get_locality, 
				     &info->start.key);

		objectid = plug_call(kplug->o.key_ops, get_objectid, 
				     &info->start.key);

		ordering = plug_call(kplug->o.key_ops, get_ordering, 
				     &info->start.key);

		/* If the object is realized on the basis of place and it 
		   is not SD, build start key first and try to find SD. */
		if (info->start.plug->id.group != STATDATA_ITEM) {
			
			/* Build the SD key on the base of the start place. */
					
			plug_call(info->start.plug->o.key_ops, build_gener,
				  &info->object, KEY_STATDATA_TYPE, locality, 
				  ordering, objectid, 0);
			
			plug_call(info->start.plug->o.key_ops, assign, 
				  STAT_KEY(&reg->obj), &info->object);
			
			/* Find SD. */
			lookup = core->tree_ops.lookup(info->tree, 
						       &info->object,
						       LEAF_LEVEL, 
						       &info->start);
			
			if (lookup == FAILED) {
				return -EINVAL;
			} else if (lookup == ABSENT) {
				/* No SD found, create a new one. */
				
			} else {
				/* SD is found. If it is not reg40 SD or is 
				   CHECKED (register_func returns an error) 
				   relocate all reg40 item and create a new 
				   SD. */
				reg40_check_sd(&info->start, mode);
			}
		} else {
			/* We are on the SD, init it. */

		}
	} else {
		/* Key of SD was specified but SD item has not been found. 
		   Create a new one.*/
		kplug = info->object.plug;
		
		locality = plug_call(kplug->o.key_ops, get_locality, 
				     &info->object);

		objectid = plug_call(kplug->o.key_ops, get_objectid, 
				     &info->object);

		ordering = plug_call(kplug->o.key_ops, get_ordering, 
				     &info->object);

	}
	
	/* Reg40 object (its SD item) has been openned or created. */
	while (TRUE) {
		plug_call(info->start.plug->o.key_ops, build_gener, &key,
			  KEY_FILEBODY_TYPE, locality, ordering, objectid, 
			  reg->offset);

		lookup = obj40_lookup(&reg->obj, &key, LEAF_LEVEL, &reg->body);
		
		switch(lookup) {
		case PRESENT:
			/* Get the maxreal key of the foudn item and find next. */
			if ((res = core->tree_ops.fetch(info->tree, &reg->body)))
				return res;
			
			if ((res = plug_call(reg->body.plug->o.item_ops, 
					     maxreal_key, &reg->body, &key)))
				return res;
			
			reg->offset = plug_call(key.plug->o.key_ops, get_offset, 
						&key) + 1;
			
			continue;
		case ABSENT:
			/* If place is invalid, then no more reg40 body items exists. */
			if (!core->tree_ops.valid(info->tree, &reg->body))
				break;
			
			/* Initializing item entity at @next place */
			if ((res = core->tree_ops.fetch(info->tree, &reg->body)))
				return res;
			
			/* Check if this is an item of another object. */
			if (plug_call(key.plug->o.key_ops, compshort, 
				      &key, &reg->body.key))
			{
				break;
			}

			/* Insert the hole. */

		case FAILED:
			return -EINVAL;
		}

		break;
	}

	return 0;

 error_free_reg:

	return -EINVAL;
}

#endif

