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
extern errno_t reg40_reset(object_entity_t *entity);
extern errno_t reg40_create_stat(obj40_t *obj, uint64_t sd);
extern errno_t reg40_holes(object_entity_t *entity);

static errno_t reg40_check_extentions(place_t *sd) {
	uint64_t mask, extmask;
	
	/*  SD may contain LW and UNIX extentions only. 
	    FIXME-VITALY: tail policy is not supported yet. */
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID);
	
	if ((extmask = obj40_extmask(sd)) == MAX_UINT64)
		return -EINVAL;
	
	return mask == extmask ? RE_OK : RE_FATAL;
}

/* Check SD extentions and that mode in LW extention is REGFILE. */
static errno_t callback_stat(place_t *sd) {
	sdext_lw_hint_t lw_hint;
	uint64_t reg;
	errno_t res;
	
	if ((res = reg40_check_extentions(sd)))
		return res;
	
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

/* Build the @obj->info.start on the basis of @obj->info.start place. */
static errno_t callback_key(obj40_t *obj) {
	uint64_t type, locality, objectid, ordering;
	
	locality = plug_call(obj->info.object.plug->o.key_ops,
			     get_locality, &obj->info.start.key);
		
	objectid = plug_call(obj->info.object.plug->o.key_ops,
			     get_objectid, &obj->info.start.key);
	
	ordering = plug_call(obj->info.object.plug->o.key_ops,
			     get_ordering, &obj->info.start.key);
	
	plug_call(obj->info.object.plug->o.key_ops, build_gener, 
		  &obj->info.object, KEY_STATDATA_TYPE, locality, 
		  ordering, objectid, 0);
	
	return 0;
}

object_entity_t *reg40_realize(object_info_t *info) {
	reg40_t *reg;
	errno_t res;
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, core, info);
	
	if ((res = obj40_realize(&reg->obj, callback_stat, callback_key)))
		goto error;
	
	/* Reseting file (setting offset to 0) */
	reg40_reset((object_entity_t *)reg);

	return (object_entity_t *)reg;
 error:
	aal_free(reg);
	return res < 0 ? INVAL_PTR : NULL;
}

static errno_t reg40_check_key(place_t *place, key_entity_t *key) {
	/* Fix SD's key if differs. */
	if (key->plug->o.key_ops->compfull(key, &place->key))
		return core->tree_ops.ukey(place, key);
	
	return 0;
}

static errno_t reg40_check_stat(place_t *stat, sdext_lw_hint_t *lw_hint, 
				uint64_t bytes, uint8_t mode) 
{
	errno_t res;

	if ((res = reg40_check_mode(stat, mode)))
		return res;
	
	return RE_OK;
}

static errno_t reg40_recreate_stat(reg40_t *reg) {
	key_entity_t *key;
	uint64_t pid;
	errno_t res;
	
	key = &reg->obj.info.object;
	
	aal_exception_error("Regular file [%s] does not have StatData "
			    "item. Creating a new one. Plugin %s.",
			    core->key_ops.print_key(key, 0), 
			    reg->obj.plug->label);
	
	pid = core->tree_ops.profile(reg->obj.info.tree, "statdata");
	
	if (pid == INVAL_PID)
		return -EINVAL;
	
	/* SD not found, create a new one. Special case and not used in 
	   reg40. Usualy objects w/out SD are skipped as they just fail 
	   to realize themselves. */
	if ((res = reg40_create_stat(&reg->obj, pid))) {
		aal_exception_error("Regular file [%s] failed to create "
				    "StatData item. Plugin %s.",
				    core->key_ops.print_key(key, 0),
				    reg->obj.plug->label);
	}
	
	return res;
}

errno_t reg40_check_struct(object_entity_t *object, 
			   place_func_t register_func,
			   uint8_t mode, void *data)
{
	uint64_t locality, objectid, ordering;
	uint64_t bytes, offset, next;
	reg40_t *reg = (reg40_t *)object;
	sdext_lw_hint_t lw_hint;
	object_info_t *info;
	errno_t res = RE_OK;
	key_entity_t key;
	lookup_t lookup;

	aal_assert("vpf-1126", reg != NULL);
	aal_assert("vpf-1190", reg->obj.info.tree != NULL);
	aal_assert("vpf-1197", reg->obj.info.object.plug != NULL);
	
	info = &reg->obj.info;
	
	/* Update the place of SD. */
	lookup = core->tree_ops.lookup(info->tree, &info->object,
				       LEAF_LEVEL, &info->start);
	
	if (lookup == FAILED)
		return -EINVAL;
	
	if (lookup == ABSENT) {
		/* If SD is not correct. Create a new one. */
		res = obj40_check_stat(&reg->obj, reg40_check_extentions);

		if (res < 0)
			return res;
		
		if (res) {
			if ((res = reg40_recreate_stat(reg)))
				return res;
		}
	} else {
		/* If SD is not correct. Fix it if needed. */
		uint64_t mask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
		uint64_t extmask;
		
		if ((extmask = obj40_extmask(&info->start)) == MAX_UINT64)
			return -EINVAL;
		
		if (extmask != mask) {
			aal_exception_error("Node (%llu), item (%u): statdata "
					    "has unknown set of extentions "
					    "(0x%llx), should be (0x%llx). "
					    "Plugin (%s)", info->start.con.blk, 
					    info->start.pos.item, 
					    obj40_extmask(&info->start), mask,
					    info->start.plug->label);
			return RE_FATAL;
		}
	}
	
	if (register_func && register_func(object, &info->start, data))
		/* Fails to register SD as an item of this file. */
		return RE_FATAL;
	
	/* Fix SD's key if differs. */
	if ((res = reg40_check_key(&info->start, &info->object))) {
		aal_exception_error("Node (%llu), item(%u): update of the "
				    "item key failed.", info->start.con.blk,
				    info->start.pos.unit);
		return res;
	}
	
	/* Build the start key of the body. */
	plug_call(info->start.plug->o.key_ops, build_gener, &key,
		  KEY_FILEBODY_TYPE, locality, ordering, objectid, 
		  reg->offset);
	
	aal_memset(&lw_hint, 0, sizeof(lw_hint));
	next = 0;
	
	/* Reg40 object (its SD item) has been openned or created. */
	while (TRUE) {
		if ((lookup = obj40_lookup(&reg->obj, &key, LEAF_LEVEL, 
					   &reg->body)) == FAILED)
			return -EINVAL;

		if (lookup == ABSENT) {
			/* If place is invalid, no more reg40 items. */
			if (!core->tree_ops.valid(info->tree, &reg->body))
				break;
			
			/* Initializing item entity at @next place */
			if ((res = core->tree_ops.fetch(info->tree, &reg->body)))
				return res;
			
			/* Check if this is an item of another object. */
			if (plug_call(key.plug->o.key_ops, compshort, 
				      &key, &reg->body.key))
				break;
		}
		
		offset = plug_call(key.plug->o.key_ops, get_offset, 
				   &reg->body.key);
		
		/* If items was reached once, skip registering and fixing. */
		if (next && next != offset) {
			/* Try to register this item. */
			if (register_func && register_func(object, &reg->body,
							   data)) 
			{
				aal_exception_error("Node (%llu), item (%u): "
						    "registering the item "
						    "failed.", reg->body.con.blk,
						    reg->body.pos.unit);

				return -EINVAL;
			}

			/* Fix item key if differs. */
			if ((res = reg40_check_key(&reg->body, &key))) {
				aal_exception_error("Node (%llu), item(%u): "
						    "update of the item key "
						    "failed.", reg->body.con.blk,
						    reg->body.pos.unit);

				return res;
			}
		} 

		reg->bplug = reg->body.plug;
		
		/* If we found not we looking foe, insert the hole. */
		if (reg->offset != offset) {
			/* Save the offset -- this item is registered once. */
			next = offset;
			
			/* This should work correctly with extents and 
			   put there flags for newly inserted items. */
			if ((res = reg40_holes(object))) {
				aal_exception_error("The object [%s] failed to "
						    "create the hole on offsets"
						    " [%llu-%llu]. Plugin %s.",
						    core->key_ops.print_key(&info->object, 0),
						    reg->offset, offset,
						    reg->obj.plug->label);
				return res;
			}
			
			/* Scan through all just created holes. */
			continue;
		} else
			next = 0;
		
		/* Count size and bytes. */
		lw_hint.size += plug_call(reg->body.plug->o.item_ops, 
					  size, &reg->body);
		
		bytes += plug_call(reg->body.plug->o.item_ops, 
				   bytes, &reg->body);
		
		/* Get the maxreal key of the found item and find next. */
		if ((res = plug_call(reg->body.plug->o.item_ops, 
				     maxreal_key, &reg->body, &key)))
			return res;
		
		reg->offset = plug_call(key.plug->o.key_ops, 
					get_offset, &key) + 1;
		
		/* Build the start key of the body. */
		plug_call(info->start.plug->o.key_ops, set_offset, 
			  &key, reg->offset);
	}
	
	/* Fix the SD -- mode, lw and unix extentions. */
	
	return reg40_check_stat(&info->start, &lw_hint, bytes, mode);
}

#endif

