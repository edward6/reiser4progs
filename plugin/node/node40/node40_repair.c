/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40_repair.c -- reiser4 node with short keys. */

#ifndef ENABLE_MINIMAL
#include "node40.h"
#include <repair/plugin.h>

#define MIN_ITEM_LEN	1

static void node40_set_offset_at(reiser4_node_t *node, int pos,
				 uint16_t offset)
{
	if (pos > nh_get_num_items(node))
		return;
    
	if (nh_get_num_items(node) == pos) {
		nh_set_free_space_start(node, offset);
	} else {
		ih_set_offset(node40_ih_at(node, pos),
			      offset, node->keypol);
	}
}

errno_t node40_region_delete(reiser4_node_t *node,
			     uint16_t start_pos, uint16_t end_pos)
{
	uint32_t count;
	uint32_t len;
	pos_t pos;
	uint16_t i;
	void *ih;
     
	aal_assert("vpf-201", node != NULL);
	aal_assert("vpf-202", node->block != NULL);
	aal_assert("vpf-213", start_pos <= end_pos);
	aal_assert("vpf-214", end_pos <= nh_get_num_items(node));
    
	ih = node40_ih_at(node, start_pos);

	for (i = start_pos; i < end_pos; i++) {
		ih_set_offset(ih, ih_get_offset(ih + ih_size(node->keypol),
						node->keypol) + 1, 
			      node->keypol);
		
		ih -= ih_size(node->keypol);
	}
    
	pos.unit = MAX_UINT32;
	pos.item = start_pos - 1;

	count = end_pos - pos.item;
	len = node40_size(node, &pos, count);

	return node40_shrink((reiser4_node_t *)node, &pos, len, count);
}

/* Count is valid if:
   free_space_start + free_space == block_size - count * ih size */
int node40_count_valid(reiser4_node_t *node) {
	uint32_t count;
	
	count = nh_get_num_items(node);
	
	if (count > node->block->size / ih_size(node->keypol))
		return 0;
	
	if (nh_get_free_space_start(node) > node->block->size)
		return 0;
	
	if (nh_get_free_space(node) > node->block->size)
		return 0;

	return (nh_get_free_space_start(node) + nh_get_free_space(node) + 
		count * ih_size(node->keypol) == node->block->size);
}

uint32_t node40_estimate_count_common(reiser4_node_t *node,
				      uint32_t node_header_size)
{
	uint32_t offset, left, right;
	uint32_t last, i;

	left = node_header_size;
	right = node->block->size - ih_size(node->keypol) - MIN_ITEM_LEN;
	last = 0;

	for (i = 0 ; ; i++, left += MIN_ITEM_LEN, 
	     right -= ih_size(node->keypol)) 
	{
		if (left > right)
			break;
		
		offset = ih_get_offset(node40_ih_at(node, i), node->keypol);
		
		if (offset >= left && offset <= right) {
			last = i;
			left = offset;
		}
	}

	return last + 1;
}

/* Look through ih array looking for the last valid item location. This will
   be the last valid item. */
static uint32_t node40_estimate_count(reiser4_node_t *node)
{
	return node40_estimate_count_common(node, sizeof(node40_header_t));
}

errno_t node40_space_check(reiser4_node_t *node, uint32_t offset, uint8_t mode)
{
	errno_t res = 0;
	uint32_t space;

	space = nh_get_free_space_start(node);
	
	/* Last relable position is not free space spart. Correct it. */
	if (offset != space) {
		/* There is left region with broken offsets, remove it. */
		fsck_mess("Node (%llu): Free space start (%u) is wrong. "
			  "Should be (%u). %s",
			  (unsigned long long)node->block->nr, space,
			  offset, mode == RM_BUILD ? "Fixed." : "");
		
		if (mode == RM_BUILD) {
			nh_set_free_space(node, nh_get_free_space(node) +
					  space - offset);
			
			nh_set_free_space_start(node, offset);
			node40_mkdirty(node);
		} else {
			res |= RE_FATAL;
		}
	}
	
	space = node->block->size - nh_get_free_space_start(node) - 
		ih_size(node->keypol) * nh_get_num_items(node);
	
	if (space != nh_get_free_space(node)) {
		/* Free space is wrong. */
		fsck_mess("Node (%llu): the free space (%u) is wrong. "
			  "Should be (%u). %s",
			  (unsigned long long)node->block->nr,
			  nh_get_free_space(node), space,
			  mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode == RM_CHECK) {
			res |= RE_FIXABLE;
		} else {
			nh_set_free_space(node, space);
			node40_mkdirty(node);
		}
	}

	return res;
}

errno_t node40_ih_array_check_common(reiser4_node_t *node, uint8_t mode,
				     uint32_t node_header_size)
{
	uint32_t right, left, offset;
	uint32_t last_pos, count, i;
	errno_t res = 0;
	bool_t relable;
	blk_t blk;
	
	aal_assert("vpf-208", node != NULL);
	aal_assert("vpf-209", node->block != NULL);

	blk = node->block->nr;
	
	offset = 0;
	last_pos = 0;
	count = nh_get_num_items(node);

	left = node_header_size;
	right = node->block->size - count * ih_size(node->keypol);
	
	for(i = 0; i <= count; i++, left += MIN_ITEM_LEN) {
		offset = (i == count) ? nh_get_free_space_start(node) : 
			ih_get_offset(node40_ih_at(node, i), node->keypol);
		
		if (i == 0) {
			if (offset == left)
				continue;
			
			fsck_mess("Node (%llu), item (0): Offset (%u) is "
				  "wrong. Should be (%u). %s",
				  (unsigned long long)blk, offset,
				  left, mode == RM_BUILD ? "Fixed." : "");

			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				continue;
			}
			
			ih_set_offset(node40_ih_at(node, 0), 
				      left, node->keypol);
			node40_mkdirty(node);
			continue;
		}
		
		relable = offset >= left && 
			offset + (count - i) * MIN_ITEM_LEN <= right;
		
		if (!relable) {
			/* fsck_mess("Node (%llu), item (%u): Offset (%u) "
				  "is wrong.", blk, i, offset); */
			
			res |= (mode == RM_BUILD ? 0 : RE_FATAL);
			
			if (count != i)
				continue;
		}

		/* i-th offset is ok or i == count. Removed broken items. */
		if ((last_pos != i - 1) || !relable) {
			uint32_t delta;

			fsck_mess("Node (%llu): Region of items [%d-%d] with "
				  "wrong offsets %s removed.",
				  (unsigned long long)blk, last_pos,
				  i - 1, mode == RM_BUILD ? "is" : "should be");

			if (mode == RM_BUILD) {
				delta = i - last_pos;
				count -= delta;
				right += delta * ih_size(node->keypol);

				if (node40_region_delete(node, last_pos + 1, i))
					return -EINVAL;

				i = last_pos;
			}
			
			/* DO not correct the left limit in the CHECK mode,leave
			   it the same last_relable_offset + n * MIN_ITEM_SIZE. 
			   However, correct it for the i == count to check free 
			   space correctly. */
			if (mode == RM_BUILD || !relable) {
				left = ih_get_offset(node40_ih_at(node, last_pos), 
						     node->keypol);
			}
		} else {
			/* Set the last correct offset and the keft limit. */
			last_pos = i;
			left = (i == count) ? nh_get_free_space_start(node) : 
				ih_get_offset(node40_ih_at(node, i), 
					      node->keypol);
		}
	}

	left -= (i - last_pos) * MIN_ITEM_LEN;
	
	res |= node40_space_check(node, left, mode);

	return res;
}

/*
 * Count of items is correct. Free space fields and item locations should be
 * checked/recovered if broken
 */
errno_t node40_ih_array_check(reiser4_node_t *node, uint8_t mode)
{
	return node40_ih_array_check_common(node, mode,
					    sizeof(node40_header_t));
}

errno_t node40_count_check_common(reiser4_node_t *node,
				  uint8_t mode,
				  uint32_t (*estimate_count)(reiser4_node_t *))
{
	uint32_t num, count;
	blk_t blk;

	aal_assert("vpf-802", node != NULL);
	aal_assert("vpf-803", node->block != NULL);
	
	blk = node->block->nr;
	
	/* Check the count of items. */
	if (node40_count_valid(node)) 
		return 0;
	
	/* Count is probably is not valid. Estimate the count. */
	num = estimate_count(node);
	count = nh_get_num_items(node);
	
	if (num >= count)
		return 0;
	
	fsck_mess("Node (%llu): Count of items (%u) is wrong. "
		  "Only (%u) items found.%s",
		  (unsigned long long)blk, count, num,
		  mode == RM_BUILD ? " Fixed." : "");

	/* Recover is impossible. */
	if (mode != RM_BUILD)
		return RE_FATAL;

	nh_set_num_items(node, num);
	node40_mkdirty(node);

	return 0;
}

/* Checks the count of items written in node_header. If it is wrong, it tries
   to estimate it on the base of free_space fields and recover if REBUILD mode.
   Returns FATAL otherwise. */
static errno_t node40_count_check(reiser4_node_t *node, uint8_t mode)
{
	return node40_count_check_common(node, mode, node40_estimate_count);
}

errno_t node40_iplug_check(reiser4_node_t *node, uint8_t mode) {
	uint32_t count, len;
	uint16_t pid;
	errno_t res;
	pos_t pos;
	void *ih;
	
	count = nh_get_num_items(node);
	ih = node40_ih_at(node, 0);
	pos.unit = MAX_UINT32;
	res = 0;

	for (pos.item = 0; pos.item < count; pos.item++, 
	     ih -= ih_size(node->keypol)) 
	{
		pid = ih_get_pid(ih, node->keypol);
		
		if (!node40_core->factory_ops.ifind(ITEM_PLUG_TYPE, pid)) {
			fsck_mess("Node (%llu), item (%u): the item of unknown "
				  "plugin id (0x%x) is found.%s",
				  (unsigned long long)node->block->nr,
				  pos.item, pid, mode == RM_BUILD ? " Removed." :
				  "");
			
			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				continue;
			}
			
			/* Item plugin cannot be found, remove it. */
			len = node40_size(node, &pos, 1);
			
			if ((res = node40_shrink(node, &pos, len, 1)))
				return res;
		}
	}

	return res;
}

errno_t node40_check_struct(reiser4_node_t *node, uint8_t mode) {
	errno_t res;
	
	aal_assert("vpf-194", node != NULL);
	
	/* Check the content of the node40 header. */
	if ((res = node40_count_check(node, mode))) 
		return res;

	if (nh_get_num_items(node) == 0) {
		uint32_t offset = sizeof(node40_header_t);
		return node40_space_check(node, offset, mode);
	}
	
	/* Count looks ok. Recover the item array. */
	res = node40_ih_array_check(node, mode);

	if (repair_error_fatal(res))
		return res;

	res |= node40_iplug_check(node, mode);

	return res;
}

errno_t node40_corrupt(reiser4_node_t *entity, uint16_t options) {
	int i;
	
	for(i = 0; i < nh_get_num_items(entity) + 1; i++) {
		if (aal_test_bit(&options, i)) {
			node40_set_offset_at(entity, i, 0xafff);
		}
	}
	
	return 0;
}

int64_t node40_insert_raw(reiser4_node_t *entity, pos_t *pos, 
			  trans_hint_t *hint) 
{
	aal_assert("vpf-965",  entity != NULL);
	aal_assert("vpf-966",  pos != NULL);
	aal_assert("vpf-1368", hint != NULL);
	
	return node40_modify(entity, pos, hint, 
			     hint->plug->repair->insert_raw);
}

/*
 * Pack node40 header w/out magic and padding
 */
void node40_header_pack(reiser4_node_t *entity, aal_stream_t *stream)
{
	node40_header_t *head;

	/* Node header w/out magic and padding. */
	head = nh(entity->block);
	
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
}

int32_t node40_items_pack(reiser4_node_t *entity, aal_stream_t *stream)
{
	reiser4_place_t place;
	uint16_t num;
	pos_t *pos;

	num = nh_get_num_items(entity);

	pos = &place.pos;
	pos->unit = MAX_UINT32;
	/*
	 * Pack all item headers
	 */
	for (pos->item = 0; pos->item < num; pos->item++) {
		void *ih = node40_ih_at(entity, pos->item);
		aal_stream_write(stream, ih, ih_size(entity->keypol));
	}
	/*
	 * Pack all item bodies
	 */
	for (pos->item = 0; pos->item < num; pos->item++) {
		if (node40_fetch(entity, pos, &place))
			return -EINVAL;

		if (place.plug->repair->pack) {
			/*
			 * Pack body
			 */
			if (objcall(&place, repair->pack, stream))
				return -EINVAL;
		} else {
			/*
			 * Do not pack body
			 */
			aal_stream_write(stream, node40_ib_at(entity, pos->item),
					 node40_len(entity, &place.pos));
		}
	}
	return 0;
}

errno_t node40_pack_common(reiser4_node_t *entity, aal_stream_t *stream,
			   void (*pack_header_fn)(reiser4_node_t *,
					     aal_stream_t *),
			   int32_t (*pack_items_fn)(reiser4_node_t *,
						    aal_stream_t *))
{
	int32_t ret;
	rid_t pid;

	aal_assert("umka-2596", entity != NULL);
	aal_assert("umka-2598", stream != NULL);

	pid = entity->plug->p.id.id;
	aal_stream_write(stream, &pid, sizeof(pid));
	/*
	 * Write node block number
	 */
	aal_stream_write(stream, &entity->block->nr,
			 sizeof(entity->block->nr));
	/*
	 * Pack node header
	 */
	pack_header_fn(entity, stream);
	/*
	 * Pack items
	 */
	ret = pack_items_fn(entity, stream);
	if (ret)
		return -EINVAL;
	return 0;
}

errno_t node40_pack(reiser4_node_t *entity, aal_stream_t *stream)
{
	return node40_pack_common(entity, stream,
				  node40_header_pack, node40_items_pack);
}

/*
 * Unpack node40 header w/out magic and padding
 */
int32_t node40_header_unpack(reiser4_node_t *entity, aal_stream_t *stream)
{
	uint32_t read;
	node40_header_t *head;

	head = nh(entity->block);

	read = aal_stream_read(stream, &head->num_items, 
			       sizeof(head->num_items));
	
	if (read != sizeof(head->num_items))
		goto error;
	
	read = aal_stream_read(stream, &head->free_space, 
			       sizeof(head->free_space));
	
	if (read != sizeof(head->free_space))
		goto error;
	
	read = aal_stream_read(stream, &head->free_space_start, 
			       sizeof(head->free_space_start));
	
	if (read != sizeof(head->free_space_start))
		goto error;
	
	read = aal_stream_read(stream, &head->mkfs_id, 
			       sizeof(head->mkfs_id));
	
	if (read != sizeof(head->mkfs_id))
		goto error;
	
	read = aal_stream_read(stream, &head->flush_id, 
			       sizeof(head->flush_id));
	
	if (read != sizeof(head->flush_id))
		goto error;
	
	read = aal_stream_read(stream, &head->flags, 
			       sizeof(head->flags));
	
	if (read != sizeof(head->flags))
		goto error;
	
	read = aal_stream_read(stream, &head->level, 
			       sizeof(head->level));

	if (read != sizeof(head->level))
		goto error;
	return 0;
 error:
	return -1;
}

/*
 * Unpack item headers and item bodies
 */
int32_t node40_items_unpack(reiser4_node_t *entity, aal_stream_t *stream)
{
	pos_t *pos;
	uint16_t num;
	uint32_t read;
	reiser4_place_t place;

	num = nh_get_num_items(entity);
	pos = &place.pos;
	pos->unit = MAX_UINT32;
	/*
	 * Unpack all item headers
	 */
	for (pos->item = 0; pos->item < num; pos->item++) {
		void *ih = node40_ih_at(entity, pos->item);
		read = aal_stream_read(stream, ih, ih_size(entity->keypol));

		if (read != ih_size(entity->keypol))
			goto error;
	}
	/*
	 * Unpack all item bodies
	 */
	for (pos->item = 0; pos->item < num; pos->item++) {
		if (node40_fetch(entity, pos, &place))
			goto error;
		if (place.plug->repair->unpack) {
			/*
			 * Unpack body
			 */
			if (objcall(&place, repair->unpack, stream))
				goto error;
		} else {
			void *ib = node40_ib_at(entity, pos->item);
			uint32_t len = node40_len(entity, &place.pos);
			/*
			 * Do not unpack body
			 */
			if (aal_stream_read(stream, ib, len) != (int32_t)len)
				goto error;
		}
	}
	return 0;
 error:
	return -1;
}

reiser4_node_t *
node40_unpack_common(aal_block_t *block,
		     reiser4_key_plug_t *kplug,
		     aal_stream_t *stream,
		     reiser4_node_plug_t *nplug,
		     const uint32_t magic,
		     reiser4_node_t* (*prepare_fn)(aal_block_t *,
						  reiser4_key_plug_t *),
		     int32_t (*unpack_header_fn)(reiser4_node_t *,
						 aal_stream_t *),
		     int32_t (*unpack_items_fn)(reiser4_node_t *,
						aal_stream_t *))
{
	int32_t ret;
	reiser4_node_t *entity;

	aal_assert("umka-2597", block != NULL);
	aal_assert("umka-2632", kplug != NULL);
	aal_assert("umka-2599", stream != NULL);

	if (!(entity = aal_calloc(sizeof(*entity), 0)))
		return NULL;

	if (!(entity = prepare_fn(block, kplug))) {
		aal_free(entity);
		return NULL;
	}
	node40_mkdirty(entity);
	/*
	 * Unpack the node header content
	 */
	ret = unpack_header_fn(entity, stream);
	if (ret)
		goto error;
	/*
	 * Set the magic and the pid
	 */
	nh_set_magic(entity, magic);
	nh_set_pid(entity, nplug->p.id.id);
	/*
	 * Unpack item headers and item bodies
	 */
	ret = unpack_items_fn(entity, stream);
	if (ret)
		goto error;
	return entity;
 error:
	aal_error("Can't unpack the node (%llu). "
		  "Stream is over?", (unsigned long long)block->nr);
	aal_free(entity);
	return NULL;
}

reiser4_node_t *node40_unpack(aal_block_t *block,
			     reiser4_key_plug_t *kplug,
			     aal_stream_t *stream)
{
	return node40_unpack_common(block, kplug, stream,
				    &node40_plug, NODE40_MAGIC,
				    node40_prepare,
				    node40_header_unpack,
				    node40_items_unpack);
}

static void node40_header_print(reiser4_node_t *entity, aal_stream_t *stream)
{
	uint8_t level;
	level = node40_get_level(entity);

	aal_stream_format(stream, "NODE (%llu) LEVEL=%u ITEMS=%u "
			  "SPACE=%u MKFS ID=0x%x FLUSH=0x%llx\n",
			  entity->block->nr, level, node40_items(entity),
			  node40_space(entity), nh_get_mkfs_id(entity),
			  nh_get_flush_id(entity));
}

void node40_print_common(reiser4_node_t *entity, aal_stream_t *stream,
			 uint32_t start, uint32_t count, uint16_t options,
			 void (*print_header_fn)(reiser4_node_t *,
						 aal_stream_t *))
{
	void *ih;
	char *key;
	pos_t pos;
	uint8_t pol;

	uint32_t last, num;
	reiser4_place_t place;

	aal_assert("vpf-023", entity != NULL);
	aal_assert("umka-457", stream != NULL);

	print_header_fn(entity, stream);

	pos.unit = MAX_UINT32;
	
	if (start == MAX_UINT32)
		start = 0;
	
	num = node40_items(entity);
	if (node40_count_valid(entity)) {
		last = num;
	} else {
		last = node40_estimate_count(entity);
		if (last > nh_get_num_items(entity))
			last = nh_get_num_items(entity);
	}
	
	if (count != MAX_UINT32 && last > start + count)
		last = start + count;
	
	pol = entity->keypol;
	
	/* Loop through the all items */
	for (pos.item = start; pos.item < last; pos.item++) {
		if (pos.item) {
			aal_stream_format(stream, "----------------------------"
					  "------------------------------------"
					  "--------------\n");
		}

		place.plug = NULL;
		node40_fetch(entity, &pos, &place);
		
		ih = node40_ih_at(entity, pos.item);

		key = print_key(node40_core, &place.key);
		
		aal_stream_format(stream, "#%u%s %s (%s): [%s] OFF=%u, "
				  "LEN=%u, flags=0x%x", pos.item,
				  pos.item >= num ? "D" : " ", place.plug ? 
				  reiser4_igname[place.plug->p.id.group] : 
				  "UNKN", place.plug ? place.plug->p.label :
				  "UNKN", key, ih_get_offset(ih, pol),
				  place.len, ih_get_flags(ih, pol));

		/* Printing item by means of calling item print method if it is
		   implemented. If it is not, then print common item information
		   like key, len, etc. */
		if (place.plug && place.plug->debug->print &&
		    place.body - entity->block->data + place.len < 
		    entity->block->size) 
		{
			objcall(&place, debug->print, stream, options);
		} else {
			aal_stream_format(stream, "\n");
		}
	}
	
	aal_stream_format(stream, "============================"
			  "===================================="
			  "==============\n");
}

/*
 * Prepare text node description and push it into specified @stream
 */
void node40_print(reiser4_node_t *entity, aal_stream_t *stream,
		  uint32_t start, uint32_t count, uint16_t options)
{
	return node40_print_common(entity, stream, start, count, options,
				   node40_header_print);
}

#endif

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
