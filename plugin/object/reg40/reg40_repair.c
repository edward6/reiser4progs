/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 default regular file plugin repair code. */
 
#ifndef ENABLE_STAND_ALONE
#include "reg40.h"
#include "repair/plugin.h"

extern reiser4_core_t *rcore;
extern reiser4_plug_t reg40_plug;

extern errno_t reg40_seek(object_entity_t *entity, 
			  uint64_t offset);

extern uint64_t reg40_offset(object_entity_t *entity);
extern errno_t reg40_update(object_entity_t *entity);
extern errno_t reg40_reset(object_entity_t *entity);

extern int32_t reg40_put(object_entity_t *entity,
			 void *buff, uint32_t n);

#define reg40_exts ((uint64_t)1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID)

static errno_t reg40_extentions(place_t *stat) {
	uint64_t extmask;
	
	/* Check that there is no one unknown extention. */
	extmask = obj40_extmask(stat);
	
	if (extmask & ~(reg40_exts | 1 << SDEXT_PLUG_ID))
		return RE_FATAL;
	
	/* Check that LW and UNIX extentions exist. */
	return ((extmask & reg40_exts) == reg40_exts) ? 0 : RE_FATAL;
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

object_entity_t *reg40_recognize(object_info_t *info) {
	reg40_t *reg;
	errno_t res;
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, rcore, info);
	
	if ((res = obj40_recognize(&reg->obj, callback_stat)))
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

static errno_t reg40_create_hole(reg40_t *reg, uint64_t len) {
	object_info_t *info = &reg->obj.info;
	uint64_t offset;
	int32_t res;

	offset = reg40_offset((object_entity_t *)reg);

	if ((res = reg40_put((object_entity_t *)reg, NULL, len)) < 0) {
		aal_exception_error("The object [%s] failed to create the hole "
				    "at [%llu-%llu] offsets. Plugin %s.",
				    print_ino(rcore, &info->object),
				    offset, offset + len, reg->obj.plug->label);
	}

	return res;
}

errno_t reg40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   region_func_t region_func,
			   void *data, uint8_t mode)
{
	uint64_t size, bytes, offset, next;
	reg40_t *reg = (reg40_t *)object;
	object_info_t *info;
	key_entity_t key;
	errno_t res = 0;

	aal_assert("vpf-1126", reg != NULL);
	aal_assert("vpf-1190", reg->obj.info.tree != NULL);
	aal_assert("vpf-1197", reg->obj.info.object.plug != NULL);
	
	info = &reg->obj.info;
	
	if ((res = obj40_stat_launch(&reg->obj, reg40_extentions, 
				     reg40_exts, 1, S_IFREG, mode)))
		return res;

	/* Try to register SD as an item of this file. */
	if (place_func && place_func(object, &info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_ukey(&reg->obj, &info->start, &info->object, mode)))
		return res;
	
	size = 0; bytes = 0; next = 0;
	
	/* Reg40 object (its SD item) has been openned or created. */
	while (TRUE) {
		if (reg40_update(object)) {
			/* If place is invalid, no more reg40 items. */
			if (!rcore->tree_ops.valid(info->tree, &reg->body))
				break;
			
			/* Initializing item entity at @next place */
			if ((res |= rcore->tree_ops.fetch(info->tree, 
							  &reg->body)))
			{
				return res;
			}
			
			/* Check if this is an item of another object. */
			if (plug_call(reg->offset.plug->o.key_ops, compshort,
				      &reg->offset, &reg->body.key))
				break;
		}
		
		offset = plug_call(reg->body.key.plug->o.key_ops,
				   get_offset, &reg->body.key);
		
		/* If items was reached once, skip registering and fixing. */
		if (!next || next != offset) {
			/* Try to register this item. Any item has a pointer 
			   to objectid in the key, if it is shared between 2 
			   objects, it should be already solved at relocation
			   time. */
			if (place_func && place_func(object, &reg->body, data))
				return -EINVAL;
		} 

		/* If we found not we looking foe, insert the hole. */
		if (reg40_offset(object) != offset) {
			if (mode == RM_BUILD) {
				/* Save offset to avoid another registering. */
				next = offset;
				
				res |= reg40_create_hole(reg, offset - 
							 reg40_offset(object));							 
				if (res < 0)
					return res;
				
				/* Scan and register created items. */
				continue;
			}
			
			aal_exception_error("The object [%s] has a break at "
					    "[%llu-%llu] offsets. Plugin %s.",
					    print_ino(rcore, &info->object),
					    reg40_offset(object), offset,
					    reg->obj.plug->label);
			res |= RE_FATAL;
		} else
			next = 0;
		
		/* Fix item key if differs. */
		if ((res |= obj40_ukey(&reg->obj, &reg->body, 
				       &reg->offset, mode)) < 0)
			return res;

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
		}
		
		/* Get the maxreal key of the found item and find next. */
		if ((res |= plug_call(reg->body.plug->o.item_ops, 
				      maxreal_key, &reg->body, &key)))
			return res;

		reg40_seek(object, plug_call(key.plug->o.key_ops, 
					     get_offset, &key) + 1);

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

void reg40_core(reiser4_core_t *c) {
	rcore = c;
}
#endif
