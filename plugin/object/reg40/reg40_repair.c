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

#define reg40_zero_extentions(sd, mask)					\
({									\
	if ((mask = obj40_extmask(sd)) == MAX_UINT64)			\
		return -EINVAL;						\
									\
	mask &= ~((uint64_t)(1 << SDEXT_UNIX_ID) | (1 << SDEXT_LW_ID) | \
		  (1 << SDEXT_PLUG_ID));				\
})

static errno_t reg40_extentions(place_t *sd) {
	uint64_t extmask;
	
	reg40_zero_extentions(sd, extmask);
	
	return extmask ? RE_FATAL : RE_OK;
}

/* Check SD extentions and that mode in LW extention is REGFILE. */
static errno_t callback_stat(place_t *sd) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = reg40_extentions(sd)))
		return res;
	
	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(sd, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	return S_ISREG(lw_hint.mode) ? 0 : RE_FATAL;
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

/* Fix place key if differs from @key. */
static errno_t reg40_ukey(reg40_t *reg, place_t *place, key_entity_t *key, 
			  uint8_t mode) 
{
	object_info_t *info;
	errno_t res;
	
	aal_assert("vpf-1218", reg != NULL);
	
	info = &reg->obj.info;
	
	if (!key->plug->o.key_ops->compfull(key, &place->key))
		return 0;
	
	aal_exception_error("Node (%llu), item(%u): the key [%s] of the item "
			    "is wrong, %s [%s]. Plugin (%s).", 
			    place->block->nr, place->pos.unit, 
			    core->key_ops.print(&place->key, PO_DEF),
			    mode == RM_BUILD ? "fixed to" : "should be", 
			    core->key_ops.print(key, PO_DEF), 
			    reg->obj.plug->label);
	
	if (mode != RM_BUILD)
		return RE_FATAL;
	
	if ((res = core->tree_ops.ukey(info->tree, place, key))) {
		aal_exception_error("Node (%llu), item(%u): update of the "
				    "item key failed.", place->block->nr,
				    place->pos.unit);
	}

	return res;
}

static errno_t reg40_recreate_stat(reg40_t *reg, uint8_t mode) {
	key_entity_t *key;
	uint64_t pid;
	errno_t res;
	
	key = &reg->obj.info.object;
	
	aal_exception_error("Regular file [%s] does not have StatData "
			    "item. %s Plugin %s.", 
			    core->key_ops.print(key, PO_DEF), 
			    mode == RM_BUILD ? "Creating a new one." : "",
			    reg->obj.plug->label);
	
	if (mode != RM_BUILD)
		return RE_FATAL;
	
	pid = core->profile_ops.value("statdata");
	
	if (pid == INVAL_PID)
		return -EINVAL;
	
	/* SD not found, create a new one. Special case and not used in 
	   reg40. Usualy objects w/out SD are skipped as they just fail 
	   to realize themselves. */
	if ((res = reg40_create_stat(&reg->obj, pid))) {
		aal_exception_error("Regular file [%s] failed to create "
				    "StatData item. Plugin %s.",
				    core->key_ops.print(key, PO_DEF),
				    reg->obj.plug->label);
	}
	
	return res;
}

typedef struct layout_hint {
	object_entity_t *entity;
	region_func_t region_func;
	void *data;
} layout_hint_t;

static errno_t callback_layout(void *p, uint64_t start, uint64_t count, 
			       void *data)
{
	layout_hint_t *hint = (layout_hint_t *)data;
	place_t *place = (place_t *)p;

	if (!start)
		return 0;

	return hint->region_func(hint->entity, start, count, hint->data);
}


errno_t reg40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   region_func_t region_func,
			   uint8_t mode, void *data)
{
	uint64_t locality, objectid, ordering;
	uint64_t size, bytes, offset, next;
	reg40_t *reg = (reg40_t *)object;
	object_info_t *info;
	errno_t res, result = RE_OK;
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
		if ((res = obj40_stat(&reg->obj, reg40_extentions)) < 0)
			return res;
		
		if (res && (res = reg40_recreate_stat(reg, mode)))
			return res;
	} else {
		/* If SD is not correct. Fix it if needed. */
		uint64_t extmask;
		
		reg40_zero_extentions(&info->start, extmask);
		
		if (extmask) {
			aal_exception_error("Node (%llu), item (%u): statdata "
					    "has unknown set of extentions "
					    "(0x%llx). Plugin (%s)", 
					    info->start.block->nr, 
					    info->start.pos.item, extmask,
					    info->start.plug->label);
			return RE_FATAL;
		}
	}
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(object, &info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((result |= reg40_ukey(reg, &info->start, &info->object, mode)))
		return result;
	
	/* Build the start key of the body. */
	plug_call(info->start.plug->o.key_ops, build_gener, &key,
		  KEY_FILEBODY_TYPE, locality, ordering, objectid, 
		  reg->offset);
	
	size = 0; bytes = 0; next = 0;
	
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
			/* Try to register this item. Any item has a pointer 
			   to objectid in the key, if it is shared between 2 
			   objects, it should be already solved at relocation
			   time. */
			if (place_func && place_func(object, &reg->body, data))
				return -EINVAL;

			/* Fix item key if differs. */
			if ((result |= reg40_ukey(reg, &reg->body, 
						  &key, mode)) < 0)
				return result;
		} 

		reg->bplug = reg->body.plug;
		
		/* If we found not we looking foe, insert the hole. */
		if (reg->offset != offset) {
			if (mode == RM_BUILD) {
				/* Save offset to avoid another registering. */
				next = offset;
				
				/* This should work correctly with extents and 
				   put there flags for newly inserted items. */
				if ((res = reg40_holes(object))) {
					aal_exception_error("The object [%s] "
							    "failed to create "
							    "the hole at [%llu"
							    "%llu] offsets. "
							    "Plugin %s.",
							    core->key_ops.print(&info->object, PO_DEF),
							    reg->offset, offset,
							    reg->obj.plug->label);
					return res;
				}

				/* Scan through all just created holes. */
				continue;
			} else {
				aal_exception_error("The object [%s] "
						    "has nothing at "
						    "[%llu-%llu] "
						    "offsets. Plugin %s.",
						    core->key_ops.print(&info->object, PO_DEF),
						    reg->offset, offset,
						    reg->obj.plug->label);
				result |= RE_FATAL;
			}
		} else
			next = 0;
		
		/* Count size and bytes. */
		size += plug_call(reg->body.plug->o.item_ops, 
					  size, &reg->body);
		
		bytes += plug_call(reg->body.plug->o.item_ops, 
				   bytes, &reg->body);
		
		/* Register object layout. */
		if (region_func && reg->body.plug->o.item_ops->check_layout) {
			layout_hint_t hint;
			
			hint.data = data;
			hint.entity = object;
			hint.region_func = region_func;
			
			if ((result |= plug_call(reg->body.plug->o.item_ops, 
						 check_layout, &reg->body, 
						 callback_layout, &hint, 
						 mode)) < 0)
				return result;
		}
		
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
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(result & RE_FATAL))
		result |= obj40_check_stat(&reg->obj, mode == RM_BUILD ?
					   reg40_zero_nlink : NULL,
					   reg40_check_mode,
					   reg40_check_size, 
					   size, bytes, mode);

	return result;
}

#endif
