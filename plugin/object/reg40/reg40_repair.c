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

extern int32_t reg40_put(object_entity_t *entity,
			 void *buff, uint32_t n);

#define known_extentions ((uint64_t)1 << SDEXT_UNIX_ID | 	\
			  	    1 << SDEXT_LW_ID |		\
				    1 << SDEXT_PLUG_ID)

static errno_t reg40_extentions(place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	extmask &= ~known_extentions;

	return extmask ? RE_FATAL : RE_OK;
}

/* Check SD extentions and that mode in LW extention is REGFILE. */
static errno_t callback_stat(place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = reg40_extentions(stat)))
		return res;
	
	/* Check the mode in the LW extention. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)) < 0)
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

typedef struct layout_hint {
	object_entity_t *entity;
	region_func_t region_func;
	void *data;
} layout_hint_t;

static errno_t callback_layout(void *p, uint64_t start, uint64_t count, 
			       void *data)
{
	layout_hint_t *hint = (layout_hint_t *)data;

	if (!start)
		return 0;

	return hint->region_func(hint->entity, start, count, hint->data);
}

static void reg40_check_mode(uint16_t *mode) {
        if (!S_ISREG(*mode)) {
                *mode &= ~S_IFMT;
                *mode |= S_IFREG;
        }
}
                                                                                           
static void reg40_check_size(uint64_t *sd_size, uint64_t counted_size) {
        /* FIXME-VITALY: This is not correct for extents as the last
           block can be not used completely. Where to take the policy
           plugin to figure out if the size is correct? */
        if (*sd_size < counted_size)
                *sd_size = counted_size;
}
                                                                                           
/* Zero nlink number for BUILD mode. */
static void reg40_zero_nlink(uint32_t *nlink) {
        *nlink = 0;
}

static errno_t reg40_create_hole(reg40_t *reg, uint64_t offset) {
	object_info_t *info = &reg->obj.info;
	errno_t res;

	/* FIXME-UMKA->VITALY: Fix third param (hole size) */
	if ((res = reg40_put((object_entity_t *)reg, NULL, 0))) {
		aal_exception_error("The object [%s] failed to create the hole "
				    "at [%llu-%llu] offsets. Plugin %s.",
				    print_ino(core, &info->object),
				    reg->offset, offset, reg->obj.plug->label);
	}

	return res;
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
	errno_t res = RE_OK;
	key_entity_t key;
	lookup_t lookup;

	aal_assert("vpf-1126", reg != NULL);
	aal_assert("vpf-1190", reg->obj.info.tree != NULL);
	aal_assert("vpf-1197", reg->obj.info.object.plug != NULL);
	
	info = &reg->obj.info;
	
	if ((res = obj40_stat_launch(&reg->obj, reg40_extentions, 
				     1, S_IFREG, mode)))
		return res;

	/* Try to register SD as an item of this file. */
	if (place_func && place_func(object, &info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_ukey(&reg->obj, &info->start, &info->object, mode)))
		return res;
	
	locality = plug_call(info->object.plug->o.key_ops,
			     get_locality, &info->object);
	
	objectid = plug_call(info->object.plug->o.key_ops,
			     get_objectid, &info->object);

	ordering = plug_call(info->object.plug->o.key_ops,
			     get_ordering, &info->object);

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
			if ((res |= core->tree_ops.fetch(info->tree, &reg->body)))
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
			if ((res |= obj40_ukey(&reg->obj, &reg->body, 
					       &key, mode)) < 0)
				return res;
		} 

		reg->bplug = reg->body.plug;
		
		/* If we found not we looking foe, insert the hole. */
		if (reg->offset != offset) {
			if (mode == RM_BUILD) {
				/* Save offset to avoid another registering. */
				next = offset;
				
				if ((res |= reg40_create_hole(reg, offset)))
					return res;
				
				/* Scan and register created items. */
				continue;
			}
			
			aal_exception_error("The object [%s] has a break at "
					    "[%llu-%llu] offsets. Plugin %s.",
					    print_ino(core, &info->object),
					    reg->offset, offset,
					    reg->obj.plug->label);
			res |= RE_FATAL;
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
			
			if ((res |= plug_call(reg->body.plug->o.item_ops, 
					      check_layout, &reg->body, 
					      callback_layout, &hint, 
					      mode)) < 0)
				return res;

			if (res & RE_FIXED) {
				/* FIXME-VITALY: mark node ditry. */
				res &= ~RE_FIXED;
			}
		}
		
		/* Get the maxreal key of the found item and find next. */
		if ((res |= plug_call(reg->body.plug->o.item_ops, 
				     maxreal_key, &reg->body, &key)))
			return res;
		
		reg->offset = plug_call(key.plug->o.key_ops, 
					get_offset, &key) + 1;
		
		/* Build the start key of the body. */
		plug_call(info->start.plug->o.key_ops, set_offset, 
			  &key, reg->offset);
	}
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL))
		res |= obj40_check_stat(&reg->obj, mode == RM_BUILD ?
					reg40_zero_nlink : NULL,
					reg40_check_mode, 
					reg40_check_size,
					size, bytes, mode);

	return res;
}

#endif
