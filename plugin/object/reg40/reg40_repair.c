/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 regular file plugin repair code. */
 
#ifndef ENABLE_STAND_ALONE
#include "reg40.h"
#include "repair/plugin.h"

#define reg40_exts ((uint64_t)1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID)

static errno_t reg40_extensions(place_t *stat) {
	uint64_t extmask;
	
	/* Check that there is no one unknown extension. */
	extmask = obj40_extmask(stat);
	
	/*
	if (extmask & ~(reg40_exts | 1 << SDEXT_PLUG_ID))
		return RE_FATAL;
	*/
	/* Check that LW and UNIX extensions exist. */
	return ((extmask & reg40_exts) == reg40_exts) ? 0 : RE_FATAL;
}

/* Check SD extensions and that mode in LW extension is REGFILE. */
static errno_t callback_stat(place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = reg40_extensions(stat)))
		return res;
	
	/* Check the mode in the LW extension. */
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

static void reg40_check_mode(obj40_t *obj, uint16_t *mode) {
        if (!S_ISREG(*mode)) {
                *mode &= ~S_IFMT;
                *mode |= S_IFREG;
        }
}
                                                                                           
static void reg40_check_size(obj40_t *obj, uint64_t *sd_size, 
			     uint64_t counted_size) 
{
	reg40_t *reg = (reg40_t *)obj;
	reiser4_plug_t *plug;
	
	aal_assert("vpf-1318", reg != NULL);
	aal_assert("vpf-1318", sd_size != NULL);
	
	if (*sd_size >= counted_size)
		return;
	
	/* sd_size lt counted size, check if it is correct for extent. */
	plug = reg40_policy_plug(reg, counted_size);

	if (plug->id.group == EXTENT_ITEM) {
		/* The last extent block can be not used up. */
		if (*sd_size + STAT_PLACE(obj)->block->size > counted_size)
			return;
	}
	
	/* SD size is not correct. */
	*sd_size = counted_size;
}

/* Zero nlink number for BUILD mode. */
static void reg40_zero_nlink(obj40_t *obj, uint32_t *nlink) {
        *nlink = 0;
}

static int64_t reg40_create_hole(reg40_t *reg, uint64_t len) {
	int64_t res;

	if ((res = reg40_put((object_entity_t *)reg, NULL, len)) < 0) {
		uint64_t offset = reg40_offset((object_entity_t *)reg);
		object_info_t *info = &reg->obj.info;

		aal_error("The object [%s] failed to create the hole "
			  "at [%llu-%llu] offsets. Plugin %s.",
			  print_inode(reg40_core, &info->object),
			  offset, offset + len, reg->obj.plug->label);
	}

	return res;
}

/* Lookup for the end byte and find out the body plug for such a size. */
static reiser4_plug_t *reg40_body_plug(reg40_t *reg) {
	key_entity_t key;
	uint64_t offset;
	place_t place;
	errno_t res;
	
	aal_assert("vpf-1305", reg != NULL);
	aal_assert("vpf-1305", reg->position.plug != NULL);

	aal_memcpy(&key, &reg->position, sizeof (key));
	plug_call(key.plug->o.key_ops, set_offset, &key, MAX_UINT64);
	
	if ((obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
			  FIND_EXACT, &place)) < 0)
		return NULL;

	/* If place is invalid, there is no items of the file. */
	if (!reg40_core->tree_ops.valid(reg->obj.info.tree, &place))
		return reg40_policy_plug(reg, 0);

	/* Initializing item entity. */
	if ((res = reg40_core->tree_ops.fetch(reg->obj.info.tree, &place)))
		return NULL;

	/* Check if this is an item of another object. */
	if (plug_call(reg->position.plug->o.key_ops, compshort,
		      &reg->position, &place.key))
		return reg40_policy_plug(reg, 0);

	/* Get the maxreal key of the found item and find next. */
	if ((res = plug_call(place.plug->o.item_ops->balance,
			     maxreal_key, &place, &key)))
		return NULL;

	offset = plug_call(key.plug->o.key_ops, get_offset, &key);
	
	return reg40_policy_plug(reg, offset);
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
	
	return offset % reg->body.block->size ? RE_FATAL : 0;
}

typedef struct reg40_repair {
	reiser4_plug_t *eplug;
	reiser4_plug_t *tplug;
	reiser4_plug_t *bplug;
	reiser4_plug_t *smart;
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
		if (!reg40_core->tree_ops.valid(info->tree, &reg->body))
			goto end;

		/* Initializing item entity at @next place */
		if ((res = reg40_core->tree_ops.fetch(info->tree, &reg->body)))
			return res;

		/* Check if this is an item of another object. */
		if (plug_call(reg->position.plug->o.key_ops, compshort,
			      &reg->position, &reg->body.key))
			goto end;

		/* If non-existent position in the item, move next. */
		if (plug_call(reg->body.plug->o.item_ops->balance,
			      units, &reg->body) == reg->body.pos.unit)
		{
			place_t next;

			if ((res = reg40_core->tree_ops.next(info->tree, 
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
			  reg->obj.plug->label, reg->body.block->nr, 
			  reg->body.pos.item,
			  print_key(reg40_core, &reg->body.key),
			  reg->body.plug->label, mode == RM_BUILD ? 
			  " Removed." : "");
	} else if (reg40_check_ikey(reg)) {
		aal_error("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] has the "
			  "wrong offset.%s",
			  print_inode(reg40_core, &info->object),
			  reg->obj.plug->label, reg->body.block->nr, 
			  reg->body.pos.item,
			  print_key(reg40_core, &reg->body.key),
			  mode == RM_BUILD ? " Removed." : "");
	} else
		return 0;

	/* Rm an item with not correct key or of unknown plugin. */
	if (mode != RM_BUILD) 
		return RE_FATAL;

	hint.count = 1;

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
	aal_assert("vpf-1354", !plug_equal(reg->body.plug, repair->bplug));

	info = &reg->obj.info;

	if (mode != RM_BUILD)
		goto error;
		
	if (plug_equal(repair->bplug, repair->tplug)) {
		/* Extent found, tail should be. Change the policy to the smart 
		   one, update the body plug and if conversion is still needed,
		   convert evth to the body plug. If convertion is not needed 
		   than it is needed fot all previous items -- from 0 through 
		   the current extent's offset. */

		if (reg->policy != repair->smart) {
			reg->policy = repair->smart;

			if (!(repair->bplug = reg40_body_plug(reg)))
				return -EINVAL;
		}
		
		if (!plug_equal(repair->bplug, repair->tplug)) {
			/* Extent found, extent should be. Convert evth from 
			   the 0 through this item offset to extents. */

			/* Count of bytes - this item offset. */
			hint->count = plug_call(reg->body.key.plug->o.key_ops, 
						get_offset, &reg->body.key);

			/* If count == 0, nothing to convert. */
			if (!hint->count)
				return 0;

			aal_error("The object [%s] (%s), node (%llu), "
				  "item (%u): the found item [%s] of "
				  "the plugin (%s) does not match the "
				  "wanted plugin (%s). Convert items "
				  "from 0 offset through hint->count "
				  "to (%s) items.",
				  print_inode(reg40_core, &info->object),
				  reg->obj.plug->label, reg->body.block->nr, 
				  reg->body.pos.item,
				  print_key(reg40_core, &reg->body.key),
				  reg->body.plug->label, repair->tplug->label,
				  repair->eplug->label);

			/* Set the start key for convertion. */
			plug_call(reg->body.key.plug->o.key_ops, assign,
				  &hint->offset, &reg->position);
			plug_call(reg->body.key.plug->o.key_ops, set_offset,
				  &hint->offset, 0);

			hint->bytes = 0;

			/* Evth is to be converted. */
			repair->bytes = 0;

			return 1;
		} 
	}
	
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

 error:
	aal_error("The object [%s] (%s), node (%llu), item (%u): the "
		  "found item [%s] of the plugin (%s) does not match "
		  "the detected tail policy (%s).%s", 
		  print_inode(reg40_core, &info->object),
		  reg->obj.plug->label, reg->body.block->nr, 
		  reg->body.pos.item,
		  print_key(reg40_core, &reg->body.key),
		  reg->body.plug->label, reg->policy->label,
		  mode == RM_BUILD ? " Converted." : "");

	/* Return 1 if the conversion should be performed right now. */
	return mode == RM_BUILD ? 0 : 1;
}

/* Obtains the maxreal key of the given place.
   Returns: maxreal key if evth is ok.
   0 -- no place; MAX_UINT64 -- some error. */
static uint64_t reg40_place_maxreal(place_t *place) {
	key_entity_t key;
	errno_t res;
	
	if (!place->plug)
		return MAX_UINT64;

	/* Get the maxreal key of the found item. */
	if ((res = plug_call(place->plug->o.item_ops->balance, 
			     maxreal_key, place, &key)))
	{
		return MAX_UINT64;
	}

	return plug_call(key.plug->o.key_ops, get_offset, &key);
}

static errno_t reg40_hole_cure(object_entity_t *object, 
			       reg40_repair_t *repair, 
			       uint8_t mode) 
{
	reg40_t *reg = (reg40_t *)object;
	object_info_t *info;
	uint64_t offset;
	int64_t res;
	
	aal_assert("vpf-1355", reg != NULL);

	offset = plug_call(reg->body.key.plug->o.key_ops, 
			   get_offset, &reg->body.key);

	if (reg40_offset(object) == offset)
		return 0;

	info = &object->info;
	
	aal_error("The object [%s] has a break at [%llu-%llu] "
		  "offsets. Plugin %s.%s", 
		  print_inode(reg40_core, &info->object), 
		  reg40_offset(object), offset, 
		  reg->obj.plug->label,
		  mode == RM_BUILD ? " Writing a hole there." 
		  : "");

	if (mode != RM_BUILD)
		return RE_FATAL;

	if ((res = reg40_create_hole(reg, offset - reg40_offset(object))) < 0)
		return res;
	
	repair->bytes += res;
	
	return 0;
}

errno_t reg40_check_struct(object_entity_t *object, 
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	reg40_t *reg = (reg40_t *)object;
	reg40_repair_t repair;
	object_info_t *info;
	conv_hint_t hint;
	errno_t res = 0;
	uint64_t size;

	aal_assert("vpf-1126", reg != NULL);
	aal_assert("vpf-1190", reg->obj.info.tree != NULL);
	aal_assert("vpf-1197", reg->obj.info.object.plug != NULL);
	
	info = &reg->obj.info;
	
	if ((res = obj40_launch_stat(&reg->obj, callback_stat, 
				     reg40_exts, 1, S_IFREG, mode)))
		return res;

	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&info->start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_fix_key(&reg->obj, &info->start, &info->object, mode)))
		return res;
	
	/* Get the reg file tail policy. */
	if (!(reg->policy = obj40_plug(&reg->obj, POLICY_PLUG_TYPE, "policy")))
	{
		aal_error("The object [%s] failed to "
			  "detect the tail policy.", 
			  print_inode(reg40_core, &info->object));
		return -EINVAL;
	}
	
	aal_memset(&repair, 0, sizeof(repair));
	
	/* Get the reg file smart tail policy. */
	if (!(repair.smart = reg40_core->factory_ops.ifind(POLICY_PLUG_TYPE, 
							   TAIL_SMART_ID)))
	{
		aal_error("Failed to find the 'smart' tail policy plugin.");
		return -EINVAL;
	}
	
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

	hint.place_func = place_func;
	
	/* Reg40 object (its SD item) has been openned or created. */
	while (TRUE) {
		errno_t result;
		
		if ((result = reg40_next(object, &repair, mode)) < 0)
			return result;
		
		if (result) {
			res |= result;
			break;
		}
		
		if (reg->body.plug) {
			repair.maxreal = reg40_place_maxreal(&reg->body);

			if (repair.maxreal == MAX_UINT64)
				return -EINVAL;

			/* Prepare the convertion if needed. */
			result = plug_equal(reg->body.plug, repair.bplug) ? 1 :
				reg40_conv_prepare(reg, &hint, &repair, mode);
		} else
			result = 1;
	
		/* If result != 0 -- convertion is needed if smth was prepared. */
		if (result && hint.offset.plug) {
			if (mode == RM_BUILD) {
				result = reg40_core->tree_ops.convert(info->tree,
								      &hint);

				if (result) return result;

				/* Evth was converted, update bytes. */
				repair.bytes += hint.bytes;
			} else 
				res |= RE_FATAL;
			
			aal_memset(&hint.offset, 0, sizeof(hint.offset));
			goto next;
		}
		
		/* No more items, break out here. */
		if (!reg->body.plug) break;

		/* Try to register this item. Any item has a pointer to 
		   objectid in the key, if it is shared between 2 objects, 
		   it should be already solved at relocation  time. */
		if (place_func && place_func(&reg->body, data))
			return -EINVAL;

		/* If we found not we looking for, insert the hole. */
		if ((res |= reg40_hole_cure(object, &repair, mode)) < 0)
			return res;
		
		if ((result = obj40_fix_key(&reg->obj, &reg->body, 
					    &reg->position, mode)))
			return result;

		/* Count bytes. */
		repair.bytes += plug_call(reg->body.plug->o.item_ops->object,
					  bytes, &reg->body);
		

	next:
		/* Find the next after the maxreal key. */
		reg40_seek(object, repair.maxreal + 1);
	}
	
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		size = plug_call(reg->position.plug->o.key_ops, 
				 get_offset, &reg->position);
		
		res |= obj40_check_stat(&reg->obj, mode == RM_BUILD ?
					reg40_zero_nlink : NULL,
					reg40_check_mode, reg40_check_size,
					size, repair.bytes, mode);
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
