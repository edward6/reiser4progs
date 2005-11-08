/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 regular file plugin repair code. */
 
#ifndef ENABLE_MINIMAL

#include "reg40_repair.h"

static int reg40_check_size(reiser4_object_t *reg, 
			    uint64_t *sd_size, 
			    uint64_t counted_size) 
{
	aal_assert("vpf-1318", reg != NULL);
	aal_assert("vpf-1318", sd_size != NULL);
	
	if (*sd_size == counted_size)
		return 0;
	
	/* sd_size lt counted size, check if it is correct for extent. */
	if (reg->body_plug && reg->body_plug->id.group == EXTENT_ITEM) {
		/* The last extent block can be not used up. */
		if (*sd_size < counted_size &&
		    *sd_size + place_blksize(STAT_PLACE(reg)) > counted_size)
		{
			return 0;
		}
	}
	
	/* SD size is not correct. */
	*sd_size = counted_size;
	
	return 1;
}

static errno_t reg40_check_ikey(reiser4_object_t *reg) {	
	uint64_t offset;
	
	aal_assert("vpf-1302", reg != NULL);
	aal_assert("vpf-1303", reg->body.plug != NULL);
	
	if (reg->body.plug->id.group == TAIL_ITEM)
		return 0;
	
	if (reg->body.plug->id.group != EXTENT_ITEM)
		return -EINVAL;

	offset = plug_call(reg->body.key.plug->pl.key, 
			   get_offset, &reg->body.key);
	
	return offset % place_blksize(&reg->body) ? RE_FATAL : 0;
}

/* Returns 1 if the convertion is needed right now, 0 if should be delayed. */
static int reg40_conv_prepare(reiser4_object_t *reg, 
			      conv_hint_t *hint,
			      uint64_t maxreal, 
			      uint8_t mode)
{
	object_info_t *info;
	
	aal_assert("vpf-1348", reg != NULL);
	aal_assert("vpf-1349", hint != NULL);
	aal_assert("vpf-1353", reg->body.plug != NULL);
	
	if (plug_equal(reg->body.plug, reg->body_plug))
		return 0;

	info = &reg->info;

	if (plug_equal(reg->body.plug, info->opset.plug[OPSET_EXTENT])) {
		/* Extent found, all previous items were tails, convert all 
		   previous ones to extents. */
		hint->plug = reg->body.plug;
		
		/* Convert from 0 to this item offset bytes. */
		if (!(hint->count = plug_call(reg->body.key.plug->pl.key, 
					      get_offset, &reg->body.key)))
		{
			return 0;
		}
		
		/* Set the start key for convertion. */
		aal_memcpy(&hint->offset, &reg->position, sizeof(hint->offset));
		plug_call(reg->body.key.plug->pl.key, set_offset,
			  &hint->offset, 0);

		hint->bytes = 0;
		
		/* Convert now. */
		return 0;
	}
	
	/* The current item should be converted to the body plug. 
	   Gather all items of the same wrong plug and convert them 
	   all together at once later. */
	hint->plug = reg->body_plug;
	
	if (hint->offset.plug == NULL) {
		aal_memcpy(&hint->offset, &reg->position, sizeof(hint->offset));

		hint->bytes = 0;
	}

	/* Count of bytes 0-this item offset. */
	hint->count = maxreal + 1 - 
		plug_call(reg->body.key.plug->pl.key,
			  get_offset, &hint->offset);
	
	/* Convertion is postponed; do not bother with it for not RM_BUILD. */
	return 1;
}

static errno_t reg40_hole_cure(reiser4_object_t *reg, 
			       obj40_stat_hint_t *hint,
			       place_func_t func,
			       uint8_t mode) 
{
	trans_hint_t trans;
	uint64_t offset;
	uint64_t len;
	int64_t res;
	
	aal_assert("vpf-1355", reg != NULL);

	offset = plug_call(reg->body.key.plug->pl.key, 
			   get_offset, &reg->body.key);

	len = offset - obj40_offset(reg);
	
	if (len == 0)
		return 0;

	fsck_mess("The object [%s] has a break at [%llu-%llu] offsets. "
		  "Plugin %s.%s", print_inode(obj40_core, &reg->info.object),
		  offset - len, offset, reiser4_oplug(reg)->label,
		  mode == RM_BUILD ? " Writing a hole there." : "");

	if (mode != RM_BUILD)
		return RE_FATAL;

	if ((res = obj40_write(reg, &trans, NULL, offset - len, 
			       len, reg->body_plug, func, NULL)) < 0)
	{
		aal_error("The object [%s] failed to create the hole "
			  "at [%llu-%llu] offsets. Plugin %s.",
			  print_inode(obj40_core, &reg->info.object),
			  offset - len, offset, reiser4_oplug(reg)->label);

		return res;
	}

	hint->bytes += trans.bytes;
	
	return 0;
}

static errno_t reg40_check_item(reiser4_object_t *reg, void *data) {
	uint8_t mode = *(uint8_t *)data;

	if (!plug_equal(reg->body.plug, reg->info.opset.plug[OPSET_EXTENT]) &&
	    !plug_equal(reg->body.plug, reg->info.opset.plug[OPSET_TAIL]))
	{
		fsck_mess("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] of the "
			  "invalid plugin (%s) found.%s",
			  print_inode(obj40_core, &reg->info.object),
			  reiser4_oplug(reg)->label, 
			  place_blknr(&reg->body), reg->body.pos.item,
			  print_key(obj40_core, &reg->body.key),
			  reg->body.plug->label, 
			  mode == RM_BUILD ? " Removed." : "");
		
		return mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	} else if (reg40_check_ikey(reg)) {
		fsck_mess("The object [%s] (%s), node (%llu),"
			  "item (%u): the item [%s] has the "
			  "wrong offset.%s",
			  print_inode(obj40_core, &reg->info.object),
			  reiser4_oplug(reg)->label, 
			  place_blknr(&reg->body), reg->body.pos.item,
			  print_key(obj40_core, &reg->body.key),
			  mode == RM_BUILD ? " Removed." : "");
		
		return mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	}
	
	return 0;
}

errno_t reg40_check_struct(reiser4_object_t *reg, 
			   place_func_t func,
			   void *data, uint8_t mode)
{
	obj40_stat_hint_t hint;
	object_info_t *info;
	conv_hint_t conv;
	uint64_t maxreal;
	uint64_t offset;
	errno_t res = 0;

	aal_assert("vpf-1126", reg != NULL);
	aal_assert("vpf-1190", reg->info.tree != NULL);
	aal_assert("vpf-1197", reg->info.object.plug != NULL);
	
	info = &reg->info;
	
	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&conv, 0, sizeof(conv));
	
	if ((res = obj40_prepare_stat(reg, S_IFREG, mode)))
		return res;

	/* Try to register SD as an item of this file. */
	if (func && func(&info->start, data))
		return -EINVAL;
	
	conv.place_func = func;
	conv.ins_hole = 1;
	maxreal = 0;
	
	/* Reg40 object (its SD item) has been opened or created. */
	while (1) {
		errno_t result;
		int the_end = 0;
		
		result = obj40_check_item(reg, reg40_check_item, NULL, &mode);
		
		if (repair_error_fatal(result))
			return result;
		else if (result == ABSENT)
			the_end = 1;
		
		offset = maxreal;
		
		if (!the_end) {
			maxreal = obj40_place_maxreal(&reg->body);
			
			if (!reg->body_plug)
				reg->body_plug = reg->body.plug;
			
			if (maxreal < offset) {
				aal_bug("vpf-1865", "The position "
					"offset is overflowed: [%s].",
					print_key(obj40_core, &reg->position));
			}
			
			if (plug_call(reg->position.plug->pl.key,
				      compfull, &reg->position,
				      &reg->body.key) > 0)
			{
				/* If in the middle of the item, go to the 
				   next. It may happen after the tail->extent
				   convertion. */
				goto next;
			}

			/* Prepare the convertion if needed. */
			result = reg40_conv_prepare(reg, &conv, maxreal, mode);
		}
	
		/* If result == 1 -- conversion is postponed;
		   If result == 0 -- conversion is not postponed anymore;
		   If conv.offset.plug != NULL, conversion was postponed. */
		if (result == 0 && conv.offset.plug) {
			offset = plug_call(conv.offset.plug->pl.key,
					   get_offset, &conv.offset);
			
			fsck_mess("The object [%s] (%s): items at offsets "
				  "[%llu..%llu] does not not match the "
				  "detected tail policy (%s).%s",
				  print_inode(obj40_core, &info->object),
				  reiser4_oplug(reg)->label, 
				  offset, offset + conv.count - 1,
				  info->opset.plug[OPSET_POLICY]->label,
				  mode == RM_BUILD ? " Converted." : "");

			if (mode == RM_BUILD) {
				if ((result = obj40_convert(reg, &conv)))
					return result;
			} else {
				res |= RE_FATAL;
			}
			
			reg->body_plug = conv.plug;
			aal_memset(&conv.offset, 0, sizeof(conv.offset));
			continue;
		}
		
		/* No more items, break out here. */
		if (the_end) break;

		/* Try to register this item. Any item has a pointer to 
		   objectid in the key, if it is shared between 2 objects, 
		   it should be already solved at relocation  time. */
		if (func && func(&reg->body, data))
			return -EINVAL;

		
		/* If conversion is postponed, do not count bytes and do not 
		   cure for holes. */
		if (conv.offset.plug)
			goto next;
		
		hint.bytes += plug_call(reg->body.plug->pl.item->object,
					bytes, &reg->body);

		/* If we found not we looking for, insert the hole. */
		if ((result = reg40_hole_cure(reg, &hint, func, mode)) < 0)
			return result;
		
next:
		/* If the file size is the max possible one, break out 
		   here to not seek to 0. */
		if (maxreal == MAX_UINT64)
			break;
		
		/* Find the next after the maxreal key. */
		obj40_seek(reg, maxreal + 1);
	}
	
	
	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		obj40_stat_ops_t ops;
	
		aal_memset(&ops, 0, sizeof(ops));
		ops.check_size = reg40_check_size;
		ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
		
		hint.mode = S_IFREG;
		hint.size = plug_call(reg->position.plug->pl.key, 
				      get_offset, &reg->position);
		
		res |= obj40_update_stat(reg, &ops, &hint, mode);
	}

	obj40_reset(reg);

	return res;
}

#endif
