/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 regular file plugin repair code. */
 
#ifndef ENABLE_STAND_ALONE

#include "reg40_repair.h"

/* Set of extentions that must present. */
#define REG40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID)

/* Set of unknown extentions. */
#define REG40_EXTS_UNKN ((uint64_t)1 << SDEXT_SYMLINK_ID)

static errno_t reg40_extensions(reiser4_place_t *stat) {
	uint64_t extmask;
	
	/* Check that there is no one unknown extension. */
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extension. */
	if (extmask & REG40_EXTS_UNKN)
		return RE_FATAL;

	/* Check that LW and UNIX extensions exist. */
	return ((extmask & REG40_EXTS_MUST) == REG40_EXTS_MUST) ? 0 : RE_FATAL;
}

/* Check SD extensions and that mode in LW extension is REGFILE. */
static errno_t callback_stat(reiser4_place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = reg40_extensions(stat)))
		return res;
	
	/* Check the mode in the LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)) < 0)
		return res;
	
	return S_ISREG(lw_hint.mode) ? 0 : RE_FATAL;

	/* FIXME: read object plug_id extention from sd. if present also. */
}

object_entity_t *reg40_recognize(object_info_t *info) {
	reg40_t *reg;
	errno_t res;
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plug, reg40_core, info);
	
	if ((res = obj40_recognize(&reg->obj, callback_stat)))
		goto error_free_reg;
	
	/* Reseting file (setting offset to 0) */
	reg40_reset((object_entity_t *)reg);

	return (object_entity_t *)reg;
	
 error_free_reg:
	aal_free(reg);
	return res < 0 ? INVAL_PTR : NULL;
}

static int reg40_check_size(obj40_t *obj, uint64_t *sd_size, 
			    uint64_t counted_size) 
{
	reg40_t *reg = (reg40_t *)obj;
	reiser4_plug_t *plug;
	
	aal_assert("vpf-1318", reg != NULL);
	aal_assert("vpf-1318", sd_size != NULL);
	
	if (*sd_size >= counted_size)
		return 0;
	
	/* sd_size lt counted size, check if it is correct for extent. */
	plug = reg40_policy_plug(reg, counted_size);

	if (plug->id.group == EXTENT_ITEM) {
		/* The last extent block can be not used up. */
		if (*sd_size + STAT_PLACE(obj)->node->block->size > counted_size)
			return 0;
	}
	
	/* SD size is not correct. */
	*sd_size = counted_size;
	return 1;
}

/* Zero nlink number for BUILD mode. */
static void reg40_zero_nlink(obj40_t *obj, uint32_t *nlink) {
        *nlink = 0;
}

/* Lookup for the end byte and find out the body plug for such a size. */
static reiser4_plug_t *reg40_body_plug(reg40_t *reg) {
	reiser4_place_t place;
	reiser4_key_t key;
	errno_t res;
	
	aal_assert("vpf-1305", reg != NULL);
	aal_assert("vpf-1305", reg->position.plug != NULL);

	aal_memcpy(&key, &reg->position, sizeof (key));
	plug_call(key.plug->o.key_ops, set_offset, &key, MAX_UINT64);
	
	if ((obj40_find_item(&reg->obj, &key, FIND_EXACT,
			     NULL, NULL, &place)) < 0)
	{
		return NULL;
	}

	/* If place is invalid, there is no items of the file. */
	if (!obj40_valid_item(&place))
		return reg40_policy_plug(reg, 0);

	/* Initializing item entity. */
	if ((res = obj40_fetch_item(&place)))
		return NULL;

	/* Check if this is an item of another object. */
	if (plug_call(reg->position.plug->o.key_ops, compshort,
		      &reg->position, &place.key))
	{
		return reg40_policy_plug(reg, 0);
	}

	/* Get the maxreal key of the found item and find next. */
	if ((res = plug_call(place.plug->o.item_ops->balance,
			     maxreal_key, &place, &key)))
	{
		return NULL;
	}

	return reg40_policy_plug(reg, plug_call(key.plug->o.key_ops,
						get_offset, &key));
}

static errno_t reg40_check_ikey(reg40_t *reg) {	
	uint64_t offset;
	
	aal_assert("vpf-1302", reg != NULL);
	aal_assert("vpf-1303", reg->body.plug != NULL);
	
	if (reg->body.plug->id.group == TAIL_ITEM)
		return 0;
	
	if (reg->body.plug->id.group != EXTENT_ITEM)
		return -EINVAL;

	offset = plug_call(reg->body.key.plug->o.key_ops, get_offset, 
			   &reg->body.key);
	
	return offset % reg->body.node->block->size ? RE_FATAL : 0;
}

typedef struct reg40_repair {
	reiser4_plug_t *eplug;
	reiser4_plug_t *tplug;
	reiser4_plug_t *bplug;
	uint64_t bytes, maxreal;
} reg40_repair_t;

static errno_t reg40_next(object_entity_t *object, 
			  reg40_repair_t *repair,
			  uint8_t mode)
{
	reg40_t *reg = (reg40_t *)object;
	object_info_t *info;
	trans_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-1344", object != NULL);
	aal_assert("vpf-1345", repair != NULL);
	
	info = &reg->obj.info;

 start:
	if ((res = reg40_update_body(object)) < 0)
		return res;

	if (res == ABSENT) {
		/* If place is invalid, no more reg40 items. */
		if (!obj40_valid_item(&reg->body))
			goto end;

		/* Initializing item entity at @next place */
		if ((res = obj40_fetch_item(&reg->body)))
			return res;

		/* Check if this is an item of another object. */
		if (plug_call(reg->position.plug->o.key_ops, compshort,
			      &reg->position, &reg->body.key))
			goto end;

		/* If non-existent position in the item, move next. */
		if (plug_call(reg->body.plug->o.item_ops->balance,
			      units, &reg->body) == reg->body.pos.unit)
		{
			reiser4_place_t next;

			/* FIXME: join this code with dir40_next. */
			if ((res = reg40_core->tree_ops.next_item(info->tree,
								  &reg->body, 
								  &next)))
				return res;

			/* If this was the last item in the tree, 
			   evth is handled. */
			if (next.node == NULL)
				goto end;

			reg->body = next;

			/* Check if this is an item of another object. */
			if (plug_call(reg->position.plug->o.key_ops, 
				      compshort, &reg->position, 
				      &reg->body.key))
				goto end;
		}
	}

	res = 0;

	if (!plug_equal(reg->body.plug, repair->eplug) && 
	    !plug_equal(reg->body.plug, repair->tplug))
	{
		aal_error("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] of the "
			  "invalid plugin (%s) found.%s",
			  print_inode(reg40_core, &info->object),
			  reg->obj.plug->label, reg->body.node->block->nr, 
			  reg->body.pos.item,
			  print_key(reg40_core, &reg->body.key),
			  reg->body.plug->label, mode == RM_BUILD ? 
			  " Removed." : "");
	} else if (reg40_check_ikey(reg)) {
		aal_error("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] has the "
			  "wrong offset.%s",
			  print_inode(reg40_core, &info->object),
			  reg->obj.plug->label, reg->body.node->block->nr, 
			  reg->body.pos.item,
			  print_key(reg40_core, &reg->body.key),
			  mode == RM_BUILD ? " Removed." : "");
	} else
		return 0;

	/* Rm an item with not correct key or of unknown plugin. */
	if (mode != RM_BUILD) 
		return RE_FATAL;

	hint.count = 1;
	hint.shift_flags = SF_DEFAULT;

	/* Item has wrong key, remove it. */
	if ((res = obj40_remove(&reg->obj, &reg->body, &hint)))
		return res;

	goto start;

 end:
	reg->body.plug = NULL;
	return 0;
}

/* Returns 1 if the convertion is needed right now, 0 if should be delayed. */
static int reg40_conv_prepare(reg40_t *reg, conv_hint_t *hint,
			      reg40_repair_t *repair, uint8_t mode)
{
	object_info_t *info;
	
	aal_assert("vpf-1348", reg != NULL);
	aal_assert("vpf-1349", hint != NULL);
	aal_assert("vpf-1350", repair != NULL);
	aal_assert("vpf-1353", reg->body.plug != NULL);
	
	if (plug_equal(reg->body.plug, repair->bplug))
		return 0;

	info = &reg->obj.info;

	if (mode != RM_BUILD)
		return 2;

	/* The current item should be converted to the body plug. 
	   Gather all items of the same wrong plug and convert them 
	   all together at once later. */
	hint->plug = repair->bplug;
	
	if (hint->offset.plug == NULL) {
		plug_call(reg->body.key.plug->o.key_ops, assign,
			  &hint->offset, &reg->position);

		hint->bytes = 0;
	}

	/* Count of bytes 0-this item offset. */
	hint->count = repair->maxreal + 1 - 
		plug_call(reg->body.key.plug->o.key_ops,
			  get_offset, &hint->offset);

	/* Convertion is postponed; do not bother with it for not RM_BUILD. */
	return mode == RM_BUILD ? 1 : 2;
}

/* Obtains the maxreal key of the given place.
   Returns: maxreal key if evth is ok.
   0 -- no place; MAX_UINT64 -- some error. */
static uint64_t reg40_place_maxreal(reiser4_place_t *place) {
	uint64_t offset, size;
	reiser4_key_t key;
	
	offset = plug_call(place->key.plug->o.key_ops, get_offset, &place->key);
	size = plug_call(place->plug->o.item_ops->object, size, place);
	
	if (offset > MAX_UINT64 - size)
		return MAX_UINT64;

	/* Get the maxreal key of the found item. */
	plug_call(place->plug->o.item_ops->balance, maxreal_key, place, &key);
	return plug_call(key.plug->o.key_ops, get_offset, &key);
}

static errno_t reg40_hole_cure(object_entity_t *object, 
			       obj40_stat_params_t *params,
			       place_func_t func,
			       uint8_t mode) 
{
	reg40_t *reg = (reg40_t *)object;
	uint64_t offset, len;
	object_info_t *info;
	int64_t res;
	
	aal_assert("vpf-1355", reg != NULL);

	offset = plug_call(reg->body.key.plug->o.key_ops, 
			   get_offset, &reg->body.key);

	if ((len = offset - reg40_offset(object)) == 0)
		return 0;

	info = &object->info;
	
	aal_error("The object [%s] has a break at [%llu-%llu] offsets. "
		  "Plugin %s.%s", print_inode(reg40_core, &info->object),
		  offset - len, offset, reg->obj.plug->label,
		  mode == RM_BUILD ? " Writing a hole there." : "");

	if (mode != RM_BUILD)
		return RE_FATAL;

	if ((res = reg40_put(object, NULL, len, func)) < 0) {
		object_info_t *info = &reg->obj.info;

		aal_error("The object [%s] failed to create the hole "
			  "at [%llu-%llu] offsets. Plugin %s.",
			  print_inode(reg40_core, &info->object),
			  offset - len, offset, reg->obj.plug->label);

		return res;
	}

	params->bytes += res;
	
	return 0;
}

errno_t reg40_check_struct(object_entity_t *object, place_func_t func,
			   void *data, uint8_t mode)
{
	reg40_t *reg = (reg40_t *)object;
	obj40_stat_methods_t methods;
	obj40_stat_params_t params;
	reg40_repair_t repair;
	object_info_t *info;
	conv_hint_t hint;
	errno_t res = 0;
	uint64_t size;

	aal_assert("vpf-1126", reg != NULL);
	aal_assert("vpf-1190", reg->obj.info.tree != NULL);
	aal_assert("vpf-1197", reg->obj.info.object.plug != NULL);
	
	info = &reg->obj.info;
	
	aal_memset(&methods, 0, sizeof(methods));
	aal_memset(&params, 0, sizeof(params));
	
	if ((res = obj40_prepare_stat(&reg->obj, S_IFREG, mode)))
		return res;

	/* Try to register SD as an item of this file. */
	if (func && func(&info->start, data))
		return -EINVAL;
	
	/* Get the reg file tail policy. */
	if (!(reg->policy = obj40_plug(&reg->obj, POLICY_PLUG_TYPE, "policy")))
	{
		aal_error("The object [%s] failed to "
			  "detect the tail policy.", 
			  print_inode(reg40_core, &info->object));
		return -EINVAL;
	}
	
	aal_memset(&repair, 0, sizeof(repair));
	
	/* Get the extent item plugin. FIXME-VITALY: param_ops.valus+ifind
	   for now untill we can point tail item in plug_extension */
	if (!(repair.eplug = obj40_plug(&reg->obj, ITEM_PLUG_TYPE, "extent"))){
		aal_error("The object [%s] failed to detect the "
			  "extent plugin to use.", 
			  print_inode(reg40_core, &info->object));
		return -EINVAL;
	}

	/* Get the tail item plugin. FIXME-VITALY: param_ops.valus+ifind
	   for now untill we can point extent item in plug_extension */
	if (!(repair.tplug = obj40_plug(&reg->obj, ITEM_PLUG_TYPE, "tail"))) {
		aal_error("The object [%s] failed to detect the "
			  "tail plugin to use.", 
			  print_inode(reg40_core, &info->object));
		return -EINVAL;
	}
	
	/* Get the maxreal file byte and find out what body plug to use. */
	if (!(repair.bplug = reg40_body_plug(reg)))
		return -EINVAL;
		
	aal_memset(&hint, 0, sizeof(hint));

	hint.place_func = func;
	
	/* Reg40 object (its SD item) has been openned or created. */
	while (1) {
		errno_t result;
		
		if ((result = reg40_next(object, &repair, mode)) < 0)
			return result;
		
		if (result) {
			res |= result;
			break;
		}
		
		if (reg->body.plug) {
			repair.maxreal = reg40_place_maxreal(&reg->body);

			if (repair.maxreal == MAX_UINT64) {
				uint64_t offset;
				
				offset = plug_call(reg->body.key.plug->o.key_ops,
						   get_offset, &reg->body.key);
				
				aal_error("The object [%s]: found item "
					  "has the wrong offset (%llu).%s",
					  print_inode(reg40_core, &info->object),
					  offset, mode != RM_CHECK ? " Removed"
					  : "");

				/* Zero the plugin as there would be no more 
				   items; there is probably a postponed 
				   convertion needs to be finished. */
				reg->body.plug = NULL;
			} else if (plug_call(reg->position.plug->o.key_ops,
					     compfull, &reg->position, 
					     &reg->body.key) > 0)
			{
				/* If in the middle of the item, go to the 
				   next. It may happen after the tail->extent
				   convertion. */
				goto next;
			} else {
				/* Prepare the convertion if needed. */
				if (!plug_equal(reg->body.plug, repair.bplug))
					result = reg40_conv_prepare(reg, &hint, 
								    &repair, mode);
			}
		}
	
		/* If result == 2 -- convertion is needed;
		   If result == 1 -- conversion is postponed;
		   If result == 0 -- conversion is not postponed anymore;
		   If hint.offset.plug != NULL, conversion was postponed. */
		if ((result == 0 && hint.offset.plug) || result == 2) {
			uint64_t offset;
			
			offset = plug_call(hint.offset.plug->o.key_ops,
					   get_offset, &hint.offset);
			
			aal_error("The object [%s] (%s): items at offsets "
				  "[%llu..%llu] does not not match the "
				  "detected tail policy (%s).%s",
				  print_inode(reg40_core, &info->object),
				  reg->obj.plug->label, offset, 
				  offset + hint.count -1, reg->policy->label,
				  mode == RM_BUILD ? " Converted." : "");

			if (mode == RM_BUILD) {
				result = reg40_core->flow_ops.convert(info->tree,
								      &hint);

				if (result) return result;

				/* Evth was converted, update bytes. */
				params.bytes += hint.bytes;
			} else {
				res |= RE_FATAL;
			}
			
			aal_memset(&hint.offset, 0, sizeof(hint.offset));
			goto next;
		}
		
		/* No more items, break out here. */
		if (!reg->body.plug) break;

		/* Try to register this item. Any item has a pointer to 
		   objectid in the key, if it is shared between 2 objects, 
		   it should be already solved at relocation  time. */
		if (func && func(&reg->body, data))
			return -EINVAL;

		
		/* If conversion is postponed, do not count bytes and do not 
		   cure for holes. */
		if (hint.offset.plug) 
			goto next;
		
		params.bytes += plug_call(reg->body.plug->o.item_ops->object,
					  bytes, &reg->body);

		/* If we found not we looking for, insert the hole. */
		if ((res |= reg40_hole_cure(object, &params, func, mode)) < 0)
			return res;
		
next:
		/* The limit is reached. */
		if (repair.maxreal == MAX_UINT64)
			break;
		
		/* Find the next after the maxreal key. */
		reg40_seek(object, repair.maxreal + 1);
	}
	
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		size = plug_call(reg->position.plug->o.key_ops, 
				 get_offset, &reg->position);
		
		params.mode = S_IFREG;
		
		methods.check_size = reg40_check_size;
		methods.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;

		res |= obj40_check_stat(&reg->obj, &methods, 
					&params, mode);
	}

	return res;
}

errno_t reg40_form(object_entity_t *object) {
	reg40_t *reg = (reg40_t *)object;

	aal_assert("vpf-1319", object != NULL);

	reg40_reset(object);
	
	/* Get the reg file tail policy. */
	if (!(reg->policy = obj40_plug(&reg->obj, POLICY_PLUG_TYPE, "policy")))
	{
		aal_error("The object [%s] failed to detect "
			  "the tail policy.", 
			  print_inode(reg40_core, &reg->obj.info.object));
		return -EINVAL;
	}

	return 0;
}
#endif
