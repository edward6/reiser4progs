/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 regular file plugin repair code. */
 
#ifndef ENABLE_MINIMAL

#include "reg40_repair.h"

/* Set of extentions that must present. */
#define REG40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID)

/* Set of unknown extentions. */
#define REG40_EXTS_UNKN ((uint64_t)1 << SDEXT_SYMLINK_ID)

object_entity_t *reg40_recognize(object_info_t *info) {
	reg40_t *reg;
	errno_t res;
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, info, reg40_core);
	
	if ((res = obj40_objkey_check(&reg->obj)))
		goto error;

	if ((res = obj40_check_stat(&reg->obj, REG40_EXTS_MUST,
				    REG40_EXTS_UNKN)))
		goto error;
	
	/* Reseting file (setting offset to 0) */
	reg40_reset((object_entity_t *)reg);

	return (object_entity_t *)reg;
	
 error:
	aal_free(reg);
	return res < 0 ? INVAL_PTR : NULL;
}

static int reg40_check_size(obj40_t *obj, uint64_t *sd_size, 
			    uint64_t counted_size) 
{
	reg40_t *reg = (reg40_t *)obj;
	
	aal_assert("vpf-1318", reg != NULL);
	aal_assert("vpf-1318", sd_size != NULL);
	
	if (*sd_size == counted_size)
		return 0;
	
	/* sd_size lt counted size, check if it is correct for extent. */
	if (reg->body_plug && reg->body_plug->id.group == EXTENT_ITEM) {
		/* The last extent block can be not used up. */
		if (*sd_size < counted_size &&
		    *sd_size + STAT_PLACE(obj)->node->block->size > 
		    counted_size)
		{
			return 0;
		}
	}
	
	/* SD size is not correct. */
	*sd_size = counted_size;
	
	return 1;
}

#if 0
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
#endif

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

static errno_t reg40_next(object_entity_t *object, uint8_t mode) {
	reg40_t *reg = (reg40_t *)object;
	object_info_t *info;
	trans_hint_t hint;
	errno_t res;
	
	aal_assert("vpf-1344", object != NULL);
	
	info = &reg->obj.info;

 start:
	if ((res = reg40_update_body(reg)) < 0)
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

			/* This could be joint with dir40_next. */
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

	if (!plug_equal(reg->body.plug, info->opset.plug[OPSET_EXTENT]) && 
	    !plug_equal(reg->body.plug, info->opset.plug[OPSET_TAIL]))
	{
		fsck_mess("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] of the "
			  "invalid plugin (%s) found.%s",
			  print_inode(reg40_core, &info->object),
			  object->opset.plug[OPSET_OBJ]->label, 
			  place_blknr(&reg->body), reg->body.pos.item,
			  print_key(reg40_core, &reg->body.key),
			  object->opset.plug[OPSET_OBJ]->label, 
			  mode == RM_BUILD ? " Removed." : "");
	} else if (reg40_check_ikey(reg)) {
		fsck_mess("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] has the "
			  "wrong offset.%s",
			  print_inode(reg40_core, &info->object),
			  object->opset.plug[OPSET_OBJ]->label, 
			  place_blknr(&reg->body), reg->body.pos.item,
			  print_key(reg40_core, &reg->body.key),
			  mode == RM_BUILD ? " Removed." : "");
	} else
		return 0;

	/* Rm an item with not correct key or of unknown plugin. */
	if (mode != RM_BUILD) 
		return RE_FATAL;

	aal_memset(&hint, 0, sizeof(hint));
	hint.count = 1;
	hint.shift_flags = SF_DEFAULT;
	reg->body.pos.unit = MAX_UINT32;

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
			      uint64_t maxreal, uint8_t mode)
{
	object_info_t *info;
	
	aal_assert("vpf-1348", reg != NULL);
	aal_assert("vpf-1349", hint != NULL);
	aal_assert("vpf-1353", reg->body.plug != NULL);
	
	if (plug_equal(reg->body.plug, reg->body_plug))
		return 0;

	info = &reg->obj.info;

	if (plug_equal(reg->body.plug, info->opset.plug[OPSET_EXTENT])) {
		/* Extent found, all previous items were tails, convert all 
		   previous ones to extents. */
		
		/* Convert from 0 to this item offset bytes. */
		if (!(hint->count = plug_call(reg->body.key.plug->o.key_ops, 
					      get_offset, &reg->body.key)))
			return 0;
		
		/* Set the start key for convertion. */
		plug_call(reg->body.key.plug->o.key_ops, assign,
			  &hint->offset, &reg->position);
		plug_call(reg->body.key.plug->o.key_ops, set_offset,
			  &hint->offset, 0);

		hint->bytes = 0;
		
		/* Convert now. */
		return 2;
	}
	
	/* The current item should be converted to the body plug. 
	   Gather all items of the same wrong plug and convert them 
	   all together at once later. */
	hint->plug = reg->body_plug;
	
	if (hint->offset.plug == NULL) {
		plug_call(reg->body.key.plug->o.key_ops, assign,
			  &hint->offset, &reg->position);

		hint->bytes = 0;
	}

	/* Count of bytes 0-this item offset. */
	hint->count = maxreal + 1 - 
		plug_call(reg->body.key.plug->o.key_ops,
			  get_offset, &hint->offset);
	
	/* Convertion is postponed; do not bother with it for not RM_BUILD. */
	return 1;
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
			       obj40_stat_hint_t *hint,
			       place_func_t func,
			       uint8_t mode) 
{
	reg40_t *reg = (reg40_t *)object;
	uint64_t offset, len;
	int64_t res;
	
	aal_assert("vpf-1355", reg != NULL);

	offset = plug_call(reg->body.key.plug->o.key_ops, 
			   get_offset, &reg->body.key);

	if ((len = offset - reg40_offset(object)) == 0)
		return 0;

	fsck_mess("The object [%s] has a break at [%llu-%llu] offsets. "
		  "Plugin %s.%s", print_inode(reg40_core, &object->object),
		  offset - len, offset, object->opset.plug[OPSET_OBJ]->label,
		  mode == RM_BUILD ? " Writing a hole there." : "");

	if (mode != RM_BUILD)
		return RE_FATAL;

	if ((res = reg40_put(object, NULL, len, func)) < 0) {
		aal_error("The object [%s] failed to create the hole "
			  "at [%llu-%llu] offsets. Plugin %s.",
			  print_inode(reg40_core, &object->object),
			  offset - len, offset, 
			  object->opset.plug[OPSET_OBJ]->label);

		return res;
	}

	hint->bytes += res;
	
	return 0;
}

errno_t reg40_check_struct(object_entity_t *object, place_func_t func,
			   void *data, uint8_t mode)
{
	reg40_t *reg = (reg40_t *)object;
	obj40_stat_hint_t hint;
	obj40_stat_ops_t ops;
	object_info_t *info;
	conv_hint_t conv;
	uint64_t maxreal;
	errno_t res = 0;

	aal_assert("vpf-1126", object != NULL);
	aal_assert("vpf-1190", object->tree != NULL);
	aal_assert("vpf-1197", object->object.plug != NULL);
	
	info = &reg->obj.info;
	
	aal_memset(&ops, 0, sizeof(ops));
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&conv, 0, sizeof(conv));
	
	if ((res = obj40_prepare_stat(&reg->obj, S_IFREG, mode)))
		return res;

	/* Try to register SD as an item of this file. */
	if (func && func(&info->start, data))
		return -EINVAL;
	
	conv.place_func = func;
	conv.ins_hole = 1;
	
	/* Reg40 object (its SD item) has been opened or created. */
	while (1) {
		errno_t result;
		
		if ((result = reg40_next(object, mode)) < 0)
			return result;
		
		if (result) {
			res |= result;
			break;
		}
		
		maxreal = 0;

		if (reg->body.plug) {
			maxreal = reg40_place_maxreal(&reg->body);
			
			if (!reg->body_plug)
				reg->body_plug = reg->body.plug;
			
			if (maxreal == MAX_UINT64) {
				uint64_t offset;
				
				offset = plug_call(reg->body.key.plug->o.key_ops,
						   get_offset, &reg->body.key);
				
				fsck_mess("The object [%s]: found item "
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
				result = reg40_conv_prepare(reg, &conv, 
							    maxreal, mode);
			}
		}
	
		/* If result == 2 -- convertion is needed;
		   If result == 1 -- conversion is postponed;
		   If result == 0 -- conversion is not postponed anymore;
		   If conv.offset.plug != NULL, conversion was postponed. */
		if ((result == 0 && conv.offset.plug) || result == 2) {
			uint64_t offset;
			
			offset = plug_call(conv.offset.plug->o.key_ops,
					   get_offset, &conv.offset);
			
			fsck_mess("The object [%s] (%s): items at offsets "
				  "[%llu..%llu] does not not match the "
				  "detected tail policy (%s).%s",
				  print_inode(reg40_core, &info->object),
				  object->opset.plug[OPSET_OBJ]->label, 
				  offset, offset + conv.count -1, 
				  object->opset.plug[OPSET_POLICY]->label,
				  mode == RM_BUILD ? " Converted." : "");

			if (mode == RM_BUILD) {
				if ((result = obj40_convert(&reg->obj, &conv)))
					return result;

				/* Evth was converted, update bytes. */
				hint.bytes += conv.bytes;
			} else {
				res |= RE_FATAL;
			}
			
			aal_memset(&conv.offset, 0, sizeof(conv.offset));
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
		if (conv.offset.plug)
			goto next;
		
		hint.bytes += plug_call(reg->body.plug->o.item_ops->object,
					bytes, &reg->body);

		/* If we found not we looking for, insert the hole. */
		if ((res |= reg40_hole_cure(object, &hint, func, mode)) < 0)
			return res;
		
next:
		/* The limit is reached. */
		if (maxreal == MAX_UINT64)
			break;
		
		/* Find the next after the maxreal key. */
		reg40_seek(object, maxreal + 1);
	}
	
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		hint.size = plug_call(reg->position.plug->o.key_ops, 
				      get_offset, &reg->position);
		
		hint.mode = S_IFREG;
		hint.must_exts = REG40_EXTS_MUST;
		hint.unkn_exts = REG40_EXTS_UNKN;
		ops.check_size = reg40_check_size;
		ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;

		res |= obj40_update_stat(&reg->obj, &ops, &hint, mode);
	}

	reg40_reset((object_entity_t *)reg);

	return res;
}

#endif
