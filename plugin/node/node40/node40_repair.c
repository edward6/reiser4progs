/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40_repair.c -- reiser4 node with short keys. */

#ifndef ENABLE_STAND_ALONE
#include "node40.h"
#include <repair/plugin.h>

#define MIN_ITEM_LEN	1

static void node40_set_offset_at(node40_t *node, int pos,
				 uint16_t offset)
{
	if (pos > nh_get_num_items(node))
		return;
    
	if (nh_get_num_items(node) == pos) {
		nh_set_free_space_start(node, offset);
	} else {
		uint32_t pol = node40_key_pol(node);
		ih_set_offset(node40_ih_at(node, pos),
			      offset, pol);
	}
}

static errno_t node40_region_delete(node40_t *node,
				    uint16_t start_pos, 
				    uint16_t end_pos) 
{
	void *ih;
	uint8_t i;
	pos_t pos;

	uint32_t pol;
	uint32_t len;
	uint32_t count;
     
	aal_assert("vpf-201", node != NULL);
	aal_assert("vpf-202", node->block != NULL);
	aal_assert("vpf-213", start_pos <= end_pos);
	aal_assert("vpf-214", end_pos <= nh_get_num_items(node));
    
	pol = node40_key_pol(node);
	ih = node40_ih_at(node, start_pos);

	for (i = start_pos; i < end_pos; i++) {
		ih_set_offset(ih, ih_get_offset(ih + ih_size(pol),
						pol) + 1, pol);
		ih -= ih_size(pol);
	}
    
	pos.unit = MAX_UINT32;
	pos.item = start_pos - 1;

	count = end_pos - pos.item;
	len = node40_size(node, &pos, count);

	if (node40_shrink((node_entity_t *)node, &pos, len, count)) {
		aal_bug("vpf-3062", "Node (%llu): Failed to delete "
			"the item (%d) of a region [%u..%u].",
			node->block->nr, i - pos.item, start_pos,
			end_pos);
		return -EIO;
	}
	
	return 0;    
}

static bool_t node40_item_count_valid(node40_t *node,
				      uint32_t count)
{
	uint32_t pol = node40_key_pol(node);
	uint32_t blksize = node->block->size;
	
	return !( ( blksize - sizeof(node40_header_t) ) / 
		  ( ih_size(pol) + MIN_ITEM_LEN ) 
		  < count);
}

static uint32_t node40_count_estimate(node40_t *node) {
	uint32_t num, blk_size, pol;
	
	aal_assert("vpf-804", node != NULL);
	aal_assert("vpf-806", node->block != NULL);
	
	pol = node40_key_pol(node);
	blk_size = node->block->size;
	
	/* Free space start is less then node_header + MIN_ITEM_LEN. */
	if (sizeof(node40_header_t) + MIN_ITEM_LEN > 
	    nh_get_free_space_start(node))
		return 0;
	
	/* Free space start is greater then the first item  */
	if (blk_size - ih_size(pol) < nh_get_free_space_start(node))
		return 0;
	
	/* Free space + node_h + 1 item_h + 1 MIN_ITEM_LEN should be less them 
	 * blksize. */
	if (nh_get_free_space(node) > blk_size -
	    sizeof(node40_header_t) - ih_size(pol) - MIN_ITEM_LEN)
	{
		return 0;
	}
	
	num = nh_get_free_space_start(node) + nh_get_free_space(node);
	
	/* Free space end > first item header offset. */
	if (num > blk_size - ih_size(pol))
		return 0;
	
	num = blk_size - num;
	
	/* The space between free space end and the end of block should be 
	 * divisible by item_header. */
	if (num % ih_size(pol))
		return 0;
	
	num /= ih_size(pol);
	
	if (!node40_item_count_valid(node, num))
		return 0;
	
	return num;
}

/* Count of items is correct. Free space fields and item locations should be 
 * checked/recovered if broken. */
static errno_t node40_item_check_array(node40_t *node, uint8_t mode) {
	uint32_t limit, offset, last_relable, count, i, last_pos;
	bool_t free_valid;
	errno_t res = 0;
	uint32_t pol;
	blk_t blk;
	
	aal_assert("vpf-208", node != NULL);
	aal_assert("vpf-209", node->block != NULL);
	
	offset = 0;

	blk = node->block->nr;
	pol = node40_key_pol(node);
	
	/* Free space fields cossider as valid if count calculated on the 
	   base of it matches the count ofrm the node_header. */
	count = nh_get_num_items(node);
	
	free_valid = (node40_count_estimate(node) == count);
	
	limit = free_valid ? nh_get_free_space_start(node) : 
		node->block->size - count * ih_size(pol);
	
	last_pos = 0;
	last_relable = sizeof(node40_header_t);
	for(i = 0; i <= count; i++) {
		offset = (i == count) ? nh_get_free_space_start(node) : 
			ih_get_offset(node40_ih_at(node, i), pol);
		
		if (i == 0) {
			if (offset == last_relable)
				continue;
			
			aal_error("Node (%llu), item (0): Offset "
				  "(%u) is wrong. Should be (%u). "
				  "%s", blk, offset, last_relable, 
				  mode == RM_BUILD ? "Fixed." : "");

			if (mode == RM_BUILD) {
				ih_set_offset(node40_ih_at(node, 0), 
					      last_relable, pol);

				node40_mkdirty((node_entity_t *)node);
			} else
				res |= RE_FATAL;

			continue;
		}
		
		if (offset < last_relable + (i - last_pos) * MIN_ITEM_LEN || 
		    offset + (count - i) * MIN_ITEM_LEN > limit) 
		{
			aal_error("Node (%llu), item (%u): Offset (%u) "
				  "is wrong.", blk, i, offset);
		} else {
			if ((mode == RM_BUILD) && (last_pos != i - 1)) {
				/* Some items are to be deleted. */
				aal_error("Node (%llu): Region of items "
					  "[%d-%d] with wrong offsets is "
					  "deleted.", blk, last_pos, i - 1);
				limit -= (offset - last_relable);
				count -= (i - last_pos);
				if (node40_region_delete(node, last_pos + 1, i))
					return -EINVAL;
				
				i = last_pos;
			} else {	    
				last_pos = i;
				last_relable = (i == count) ?
					nh_get_free_space_start(node) : 
					ih_get_offset(node40_ih_at(node, i), pol);
			}
		}
	}
	
	if (free_valid) {
		aal_assert("vpf-807", node40_count_estimate(node) == count);
	}
    
	/* Last relable position is not free space spart. Correct it. */
	if (last_pos != count) {	
		/* There is left region with broken offsets, remove it. */
		aal_error("Node (%llu): Free space start (%u) is wrong. "
			  "Should be (%u). %s", blk, offset, last_relable, 
			  mode == RM_BUILD ? "Fixed." : "");
		
		if (mode == RM_BUILD) {
			nh_set_free_space(node, nh_get_free_space(node) + 
					  offset - last_relable);
			
			nh_set_free_space_start(node, last_relable);
			node40_mkdirty((node_entity_t *)node);
		} else {
			res |= RE_FATAL;
		}
	}
	
	last_relable = node->block->size - last_relable - 
		ih_size(pol) * count;
	
	if (last_relable != nh_get_free_space(node)) {
		/* Free space is wrong. */
		aal_error("Node (%llu): the free space (%u) is wrong. "
			  "Should be (%u). %s", blk, 
			  nh_get_free_space(node), last_relable,
			  mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode == RM_CHECK) {
			res |= RE_FIXABLE;
		} else {
			nh_set_free_space(node, last_relable);
			node40_mkdirty((node_entity_t *)node);
		}
	}
	
	return res;
}

static errno_t node40_item_find_array(node40_t *node, uint8_t mode) {
	uint32_t offset, i, nr = 0;
	uint32_t pol;
	blk_t blk;
	
	aal_assert("vpf-800", node != NULL);
	
	blk = node->block->nr;
	pol = node40_key_pol(node);
	
	for (i = 0; ; i++) {
		offset = ih_get_offset(node40_ih_at(node, i), pol);
		
		if (i) {
			if (offset < sizeof(node40_header_t) + i * MIN_ITEM_LEN)
				break;	
		} else {
			if (offset != sizeof(node40_header_t))
				return RE_FATAL;
		}
		
		if (node->block->size - ih_size(pol) * (i + 1) -
		    MIN_ITEM_LEN < offset)
		{
			break;
		}
		
		nr++;
	}
	
	/* Only nr - 1 item can be recovered as free space start is unknown. */
	if (nr <= 1)
		return RE_FATAL;
	
	if (--nr != nh_get_num_items(node)) {
		aal_error("Node (%llu): Count of items (%u) is wrong. "
			  "Found only (%u) items. %s", blk, 
			  nh_get_num_items(node), nr, 
			  mode == RM_BUILD ? "Fixed." : "");
		
		if (mode == RM_BUILD) {
			nh_set_num_items(node, nr);
			node40_mkdirty((node_entity_t *)node);
		} else
			return RE_FATAL;
	}
	
	offset = ih_get_offset(node40_ih_at(node, nr + 1), pol);
	if (offset != nh_get_free_space_start(node)) {
		aal_error("Node (%llu): Free space start (%u) is wrong. "
			  "(%u) looks correct. %s", blk, 
			  nh_get_free_space_start(node), offset, 
			  mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode != RM_CHECK) {
			nh_set_free_space_start(node, offset);
			node40_mkdirty((node_entity_t *)node);
		} else
			return RE_FIXABLE;
	}
	
	offset = node->block->size - offset - 
		nr * ih_size(pol);
	
	if (offset != nh_get_free_space_start(node)) {
		aal_error("Node (%llu): Free space (%u) is wrong. "
			  "Should be (%u). %s", blk, 
			  nh_get_free_space(node), offset, 
			  mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode != RM_CHECK) {
			nh_set_free_space(node, offset);
			node40_mkdirty((node_entity_t *)node);
		} else
			return RE_FIXABLE;
	}
	
	return 0;
}

/* Checks the count of items written in node_header. If it is wrong, it tries
   to estimate it on the base of free_space fields and recover if REBUILD mode.
   Returns FATAL otherwise. */
static errno_t node40_count_check(node40_t *node, uint8_t mode) {
	uint32_t num;
	blk_t blk;
	
	aal_assert("vpf-802", node != NULL);
	aal_assert("vpf-803", node->block != NULL);
	
	blk = node->block->nr;
	
	if (node40_item_count_valid(node, nh_get_num_items(node)))
		return 0;
	
	/* Count is wrong. Try to recover it if possible. */
	num = node40_count_estimate(node);
	
	/* Recover is impossible. */
	if (num == 0) {
		aal_error("Node (%llu): Count of items (%u) is wrong.", 
			  blk, nh_get_num_items(node));
		return RE_FATAL;
	}
	
	/* Recover is possible. */
	aal_error("Node (%llu): Count of items (%u) is wrong. (%u) looks "
		  "correct. %s", blk, nh_get_num_items(node), num, 
		  mode == RM_BUILD ? "Fixed." : "");
	
	if (mode != RM_BUILD) 
		return RE_FATAL;
	
	nh_set_num_items(node, num);
	node40_mkdirty((node_entity_t *)node);
	
	return 0;
}

errno_t node40_check_struct(node_entity_t *entity, uint8_t mode) {
	node40_t *node = (node40_t *)entity;
	errno_t res;
	
	aal_assert("vpf-194", node != NULL);
	
	/* Check the content of the node40 header. */
	if ((res = node40_count_check(node, mode))) {
		/* Count is wrong and not recoverable on the 
		   base of free space end. */
		if (mode != RM_BUILD)
			return res;
		
		/* Recover count on the base of correct item 
		   array if one exists. */
		return node40_item_find_array(node, mode);
	}
	
	/* Count looks ok. Recover item array. */
	return node40_item_check_array(node, mode);    
}

errno_t node40_corrupt(node_entity_t *entity, uint16_t options) {
	node40_t *node;
	int i;
	
	node = (node40_t *)entity;
	
	for(i = 0; i < nh_get_num_items(node) + 1; i++) {
		if (aal_test_bit(&options, i)) {
			node40_set_offset_at(node, i, 0xffff);
		}
	}
	
	return 0;
}

int64_t node40_merge(node_entity_t *entity, pos_t *pos, trans_hint_t *hint) {
	aal_assert("vpf-965",  entity != NULL);
	aal_assert("vpf-966",  pos != NULL);
	aal_assert("vpf-1368", hint != NULL);
	
	return node40_modify(entity, pos, hint, 
			     hint->plug->o.item_ops->repair->merge);
}

void node40_set_flag(node_entity_t *entity, uint32_t pos, uint16_t flag) {
	node40_t *node;
	uint32_t pol;
	void *ih;
	
	aal_assert("vpf-1038", entity != NULL);
	
	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	ih = node40_ih_at(node, pos);

	if (ih_test_flag(ih, flag, pol))
		return;
	
	ih_set_flag(ih, flag, pol);
	node40_mkdirty(entity);
}

void node40_clear_flag(node_entity_t *entity, uint32_t pos, uint16_t flag) {
	node40_t *node;
	uint32_t pol;
	void *ih;
	
	aal_assert("vpf-1039", entity != NULL);
	
	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	ih = node40_ih_at(node, pos);
	
	if (flag == MAX_UINT16) {
		if (!ih_get_flags(ih, pol)) 
		    return;

		ih_set_flags(ih, 0, pol);
	} else {
		if (!ih_test_flag(ih, flag, pol))
			return;
		
		ih_clear_flag(ih, flag, pol);
	}
	
	node40_mkdirty(entity);
}

bool_t node40_test_flag(node_entity_t *entity, uint32_t pos, uint16_t flag) {
	node40_t *node;
	uint32_t pol;
	void *ih; 
	
	aal_assert("vpf-1040", entity != NULL);
	
	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	ih = node40_ih_at(node, pos);
	
	return flag == MAX_UINT16 ? ih_get_flags (ih, pol) == 0 : 
		ih_test_flag(ih, flag, pol);
}

errno_t node40_pack(node_entity_t *entity, aal_stream_t *stream, int mode) {
	node40_t *node = (node40_t *)entity;
	node40_header_t *head;
	place_t place;
	uint32_t pol;
	uint16_t num;
	pos_t *pos;
	rid_t pid;
	
	aal_assert("umka-2596", entity != NULL);
	aal_assert("umka-2598", stream != NULL);
	
	pid = entity->plug->id.id;
	aal_stream_write(stream, &pid, sizeof(pid));
	
	/* Write node block number. */
	aal_stream_write(stream, &entity->block->nr,
			 sizeof(entity->block->nr));

	if (mode != PACK_FULL) {
		/* Write node raw data. */
		aal_stream_write(stream, entity->block->data,
				 entity->block->size);

		return 0;
	}
	
	pol = node40_key_pol(node);
	
	/* Pack the node content. */
	
	/* Node header w/out magic and padding. */
	head = nh((node)->block);
	aal_stream_write(stream, &head->num_items, 
			 sizeof(head->num_items));
	aal_stream_write(stream, &head->free_space, 
			 sizeof(head->free_space));
	aal_stream_write(stream, &head->free_space_start, 
			 sizeof(head->free_space_start));
	aal_stream_write(stream, &head->mkfs_id, 
			 sizeof(head->mkfs_id));
	aal_stream_write(stream, &head->flush_id, 
			 sizeof(head->flush_id));
	aal_stream_write(stream, &head->flags, 
			 sizeof(head->flags));
	aal_stream_write(stream, &head->level, 
			 sizeof(head->level));

	/* All items. */
	num = nh_get_num_items(node);
	pos = &place.pos;
	pos->unit = MAX_UINT32;
	
	/* Pack all item headers. */
	for (pos->item = 0; pos->item < num; pos->item++) {
		void *ih = node40_ih_at(node, pos->item);
		aal_stream_write(stream, ih, ih_size(pol));
	}

	/* Paack all item bodies. */
	
	for (pos->item = 0; pos->item < num; pos->item++) {
		if (node40_fetch(entity, pos, &place))
			return -EINVAL;
		
		if (place.plug->o.item_ops->repair->pack) {
			/* Pack body. */
			if (plug_call(place.plug->o.item_ops->repair,
				      pack, &place, stream))
				return -EINVAL;
		} else {
			/* Do not pack body. */
			aal_stream_write(stream, node40_ib_at(node, pos->item),
					 node40_len(entity, &place.pos));
		}
	}
	
	return 0;
}

node_entity_t *node40_unpack(aal_block_t *block,
			     reiser4_plug_t *kplug,
			     aal_stream_t *stream,
			     int mode)
{
	node40_header_t *head;
	node40_t *node;
	place_t place;
	uint32_t pol;
	uint16_t num;
	pos_t *pos;

	aal_assert("umka-2597", block != NULL);
	aal_assert("umka-2632", kplug != NULL);
	aal_assert("umka-2599", stream != NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	node->kplug = kplug;
	node->block = block;
	node->plug = &node40_plug;
	
	node40_mkdirty((node_entity_t *)node);
	
	if (mode != PACK_FULL) {
		/* Read node raw data. */
		aal_stream_read(stream, node->block->data,
				node->block->size);
		
		return (node_entity_t *)node;
	}

	pol = node40_key_pol(node);
	
	/* Unpack the node content. */
	
	/* Node header w/out magic and padding. */
	head = nh((node)->block);
	aal_stream_read(stream, &head->num_items, 
			sizeof(head->num_items));
	aal_stream_read(stream, &head->free_space, 
			sizeof(head->free_space));
	aal_stream_read(stream, &head->free_space_start, 
			sizeof(head->free_space_start));
	aal_stream_read(stream, &head->mkfs_id, 
			sizeof(head->mkfs_id));
	aal_stream_read(stream, &head->flush_id, 
			sizeof(head->flush_id));
	aal_stream_read(stream, &head->flags, 
			sizeof(head->flags));
	aal_stream_read(stream, &head->level, 
			sizeof(head->level));

	/* Set the magic and the pid. */
	nh_set_magic(node, NODE40_MAGIC);
	nh_set_pid(node, node40_plug.id.id);
	
	/* All items. */
	num = nh_get_num_items(node);
	pos = &place.pos;
	pos->unit = MAX_UINT32;
	
	/* Unpack all item headers. */
	for (pos->item = 0; pos->item < num; pos->item++) {
		void *ih = node40_ih_at(node, pos->item);
		aal_stream_read(stream, ih, ih_size(pol));
	}

	/* Unpack all item bodies. */
	for (pos->item = 0; pos->item < num; pos->item++) {
		if (node40_fetch((node_entity_t *)node, pos, &place))
			return NULL;
		
		if (place.plug->o.item_ops->repair->unpack) {
			/* Unpack body. */
			if (plug_call(place.plug->o.item_ops->repair,
				      unpack, &place, stream))
				return NULL;
		} else {
			void *ib = node40_ib_at(node, pos->item);
			uint32_t len = node40_len((node_entity_t *)node, 
						  &place.pos);
			
			/* Do not unpack body. */
			aal_stream_read(stream, ib, len);
		}
	}

	
	return (node_entity_t *)node;
}

/* Prepare text node description and push it into specified @stream. */
void node40_print(node_entity_t *entity, aal_stream_t *stream,
		  uint32_t start, uint32_t count, uint16_t options) 
{
	void *ih;
	char *key;
	pos_t pos;
	uint8_t level;
	uint32_t last, pol;	
	
	place_t place;
	node40_t *node;

	aal_assert("vpf-023", entity != NULL);
	aal_assert("umka-457", stream != NULL);

	node = (node40_t *)entity;
	level = node40_get_level(entity);
	
	/* Print node header. */
	aal_stream_format(stream, "NODE (%llu) LEVEL=%u ITEMS=%u "
			  "SPACE=%u MKFS ID=0x%x FLUSH=0x%llx\n",
			  node->block->nr, level, node40_items(entity),
			  node40_space(entity), nh_get_mkfs_id(node),
			  nh_get_flush_id(node));
	
	pos.unit = MAX_UINT32;
	
	if (start == MAX_UINT32)
		start = 0;
	
	last = node40_items(entity);
	
	if (last > start + count)
		last = start + count;
	
	pol = node40_key_pol(node);
	
	/* Loop through the all items */
	for (pos.item = start; pos.item < last; pos.item++) {
		if (pos.item) {
			aal_stream_format(stream, "----------------------------"
					  "------------------------------------"
					  "--------------\n");
		}
			
		if (node40_fetch(entity, &pos, &place))
			continue;
		
		ih = node40_ih_at(node, pos.item);
		key = node40_core->key_ops.print(&place.key, PO_DEFAULT);
		
		aal_stream_format(stream, "#%u, %s (%s): [%s] OFF %u, "
				  "LEN=%u, flags=0x%x ", pos.item, 
				  reiser4_igname[place.plug->id.group],
				  place.plug->label, key, 
				  ih_get_offset(ih, pol),
				  place.len, ih_get_flags(ih, pol));

		/* Printing item by means of calling item print method if it is
		   implemented. If it is not, then print common item information
		   like key, len, etc. */
		if (place.plug->o.item_ops->debug->print) {
			plug_call(place.plug->o.item_ops->debug,
				  print, &place, stream, options);
		} else {
			aal_stream_format(stream, "\n");
		}
	}
	
	aal_stream_format(stream, "============================"
			  "===================================="
			  "==============\n");
}
#endif
