/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node_large.c -- reiser4 default node plugin. */

#ifdef ENABLE_LARGE_KEYS
#include "node_large.h"

static reiser4_core_t *core = NULL;
extern reiser4_plug_t node_large_plug;

/* Returns item header by pos */
item_header_t *node_large_ih_at(node_t *node,
				uint32_t pos)
{
	item_header_t *ih;
	aal_block_t *block;

	block = node->block;

	ih = (item_header_t *)(block->data +
			       block->size);
	
	return ih - pos - 1;
}

/* Retutrns item body by pos */
void *node_large_ib_at(node_t *node,
		       uint32_t pos)
{
	item_header_t *ih;
	aal_block_t *block;
	
	block = node->block;
	ih = node_large_ih_at(node, pos);
	
	return block->data + ih_get_offset(ih);
}

/* Returns node free space end offset */
uint16_t node_large_free_space_end(node_t *node) {
	uint32_t items = nh_get_num_items(node);

	return aal_block_size(node->block) -
		(items * sizeof(item_header_t));
}

/* Creates node_large entity on specified device and block number. This can be
   used later for working with all node methods. */
static object_entity_t *node_large_init(aal_device_t *device,
					uint32_t size, blk_t blk)
{
	object_entity_t *entity;

	if (!(entity = node_common_init(device, size, blk)))
		return NULL;
	
	entity->plug = &node_large_plug;

	return entity;
}

/* Returns key at passed @pos */
static errno_t node_large_get_key(object_entity_t *entity,
				  pos_t *pos, key_entity_t *key) 
{
	void *body;
    
	aal_assert("umka-821", key != NULL);
	aal_assert("umka-939", pos != NULL);
	
	aal_assert("umka-2344", pos->item <
		   node_common_items(entity));

	aal_assert("umka-2333", entity != NULL);
	aal_assert("umka-2022", loaded(entity));

	body = &(node_large_ih_at((node_t *)entity,
				  pos->item)->key);
	
	aal_memcpy(key->body, body, sizeof(key_t));
	return 0;
}

/* Returns length of item at pos. */
static uint16_t node_large_len(object_entity_t *entity, 
			       pos_t *pos)
{
	item_header_t *ih;
    
	aal_assert("umka-942", pos != NULL);
	aal_assert("vpf-037", entity != NULL);
	aal_assert("umka-2024", loaded(entity));

	/* Item length is calculated as next item body offset minus current item
	   offset. If we are on the last item then we use free space start for
	   that. We use this way, because reiser4 kernel code does not set
	   item's length correctly. And they are rather reserved for future
	   using. */
	ih = node_large_ih_at((node_t *)entity, pos->item);

	if (pos->item == (uint32_t)(node_common_items(entity) - 1)) {
		return nh_get_free_space_start((node_t *)entity) -
			ih_get_offset(ih);
	}

	return ih_get_offset(ih - 1) - ih_get_offset(ih);
}

/* Initializes item entity in order to pass it to an item plugin routine. If
   unit component of pos is set up the function will initialize item's key from
   the unit one. */
errno_t node_large_get_item(object_entity_t *entity,
			    pos_t *pos, place_t *place)
{
	rid_t pid;
	node_t *node;
	
	aal_assert("umka-1813", pos != NULL);
	aal_assert("umka-1602", place != NULL);
	aal_assert("umka-1631", entity != NULL);

	node = (node_t *)entity;
	
	if (pos->item >= nh_get_num_items(node))
		return -EINVAL;
	
	/* Initializes item's context (device, block number, etc) */
	place->con.blksize = node->size;
	place->con.device = node->block->device;
	place->con.blk = aal_block_number(node->block);

	/* Initializing item's plugin */
	pid = ih_get_pid(node_large_ih_at(node, pos->item));
	
	if (!(place->plug = core->factory_ops.pfind(ITEM_PLUG_TYPE,
						    pid, LARGE)))
	{
		aal_exception_error("Can't find item plugin by its id "
				    "0x%x", pid);
		return -EINVAL;
	}

	/* Initializing item's pos, body pointer and length */
	place->len = node_large_len(entity, pos);
	place->body = node_large_ib_at(node, pos->item);
	aal_memcpy(&place->pos, pos, sizeof(pos_t));

	/* FIXME-UMKA: Here should be not hardcoded plugin id */
	if (!(place->key.plug = core->factory_ops.ifind(KEY_PLUG_TYPE,
							KEY_LARGE_ID)))
	{
		aal_exception_error("Can't find key plugin by its id "
				    "0x%x", KEY_LARGE_ID);
		return -EINVAL;
	}

	/* Getting item key */
	return node_large_get_key(entity, pos, &place->key);
}

#ifndef ENABLE_STAND_ALONE
/* Retutns item overhead for this node format. Widely used in modification and
   estimation routines. */
static uint16_t node_large_overhead(object_entity_t *entity) {
	return sizeof(item_header_t);
}

/* Returns maximal size of item possible for passed node instance */
static uint16_t node_large_maxspace(object_entity_t *entity) {
	aal_assert("vpf-016", entity != NULL);

	/* Maximal space is node size minus node header and minus item
	   overhead. */
	return ((node_t *)entity)->size - sizeof(node_header_t) -
		sizeof(item_header_t);
}

/* Calculates size of a region denoted by @pos and @count. This is used by
   node_large_rep(), node_large_remove(), etc. */
static uint32_t node_large_size(node_t *node, pos_t *pos,
				uint32_t count)
{
	uint32_t len;
	uint32_t items;
	
	item_header_t *cur;

	items = nh_get_num_items(node);

	aal_assert("umka-1811", pos->item +
		   count <= items);
	
	cur = node_large_ih_at(node, pos->item);

	if (pos->item + count < items)
		len = ih_get_offset(cur - count);
	else
		len = nh_get_free_space_start(node);

	return len - ih_get_offset(cur);
}

/* Makes expand passed @node by @len in odrer to make room for insert new
   items/units. This function is used by insert and shift methods. */
errno_t node_large_expand(object_entity_t *entity, pos_t *pos,
			  uint32_t len, uint32_t count)
{
	int is_insert;
	
	node_t *node;
	uint32_t item;
	uint32_t items;
	uint32_t offset;
	uint32_t headers;
	item_header_t *ih;

	aal_assert("vpf-006", pos != NULL);
	aal_assert("umka-817", entity != NULL);

	node = (node_t *)entity;

	if (len == 0)
		return 0;
	
	/* Checks for input validness */
	is_insert = (pos->unit == MAX_UINT32);

	items = nh_get_num_items(node);
	headers = count * sizeof(item_header_t);

	aal_assert("vpf-026", nh_get_free_space(node) >= 
		   len + (is_insert ? sizeof(item_header_t) : 0));
	
	aal_assert("vpf-027", pos->item <= items);

	/* Getting real pos of the item to be updated */
	item = pos->item + !is_insert;
	ih = node_large_ih_at(node, item);

	/* If item pos is inside the range [0..count - 1], we should perform the
	   data moving and offset upadting. */
	if (item < items) {
		void *src, *dst;
		uint32_t i, size;

		offset = ih_get_offset(ih);

		/* Moving items bodies */
		src = node->block->data + offset;
		dst = src + len;

		size = nh_get_free_space_start(node) - offset;

		aal_memmove(dst, src, size);

		/* Updating item offsets */
		for (i = 0; i < items - item; i++, ih--) 
			ih_inc_offset(ih, len);

		/* If this is the insert new item mode, we should prepare the
		   room for new item header and set it up. */
		if (is_insert) {
			src = node_large_ih_at(node, items - 1);

			dst = node_large_ih_at(node, items - 1 +
					   count);

			size = sizeof(item_header_t) *
				(items - item);
			
			aal_memmove(dst, src, size);
		}

		ih = node_large_ih_at(node, item);
	} else
		offset = nh_get_free_space_start(node);

	/* Updating node's free space and free space start fields */
	nh_inc_free_space_start(node, len);
	nh_dec_free_space(node, len);

	if (is_insert) {
                /* Setting up the fields of new item */
		ih_set_offset(ih, offset);

		/* Setting up node header */
		nh_inc_num_items(node, count);
		nh_dec_free_space(node, headers);
	} else {
		/* Increasing item len mfor the case of pasting new units */
		ih = node_large_ih_at(node, pos->item);
	}
	
	node->dirty = 1;
	return 0;
}

/* General node_large cutting function. It is used from shift, remove, etc. It
   removes an amount of items specified by @count and shrinks node. */
errno_t node_large_shrink(object_entity_t *entity, pos_t *pos,
			  uint32_t len, uint32_t count)
{
	int is_range;
	uint32_t size;
	void *src, *dst;

	node_t *node;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, items;

	item_header_t *cur;
	item_header_t *end;

	aal_assert("umka-1800", count > 0);
	aal_assert("umka-1799", pos != NULL);
	aal_assert("umka-1798", entity != NULL);

	node = (node_t *)entity;
	items = nh_get_num_items(node);

	is_range = (pos->item < items);
	aal_assert("umka-1801", is_range);

	if (pos->unit == MAX_UINT32) {
		is_range = (is_range && pos->item + count <= items);
		aal_assert("umka-1802", is_range);

		end = node_large_ih_at(node, items - 1);
		headers = count * sizeof(item_header_t);

		/* Moving item header and bodies if it is needed */
		if (pos->item + count < items) {

			/* Moving item bodies */
			dst = node_large_ib_at(node, pos->item);
			src = node_large_ib_at(node, pos->item + count);

			size = node_large_size(node, pos, items -
					   pos->item) - len;
 
			aal_memmove(dst, src, size);

			/* Moving item headers */
			src = node_large_ih_at(node, items - 1);
			dst = src + headers;
			
			size = (items - (pos->item + count)) *
				sizeof(item_header_t);
	
			aal_memmove(dst, src, size);

			/* Updating item offsets */
			cur = node_large_ih_at(node, pos->item);
	
			for (i = pos->item; i < items - count; i++, cur--)
				ih_dec_offset(cur, len);
		}

		/* Updating node header */
		nh_dec_num_items(node, count);
		nh_inc_free_space(node, (len + headers));
	} else {
		uint32_t item_len;
		item_header_t *ih;
		object_entity_t *entity;

		entity = (object_entity_t *)node;

		ih = node_large_ih_at(node, pos->item);
		item_len = node_large_len(entity, pos);
		
		/* Moving item bodies */
		src = node_large_ib_at(node, pos->item) + item_len;
		dst = node_large_ib_at(node, pos->item) + item_len - len;

		size = nh_get_free_space_start(node) -
			ih_get_offset(ih) - item_len;
		
		aal_memmove(dst, src, size);
		
		/* Updating header offsets */
		end = node_large_ih_at(node, items - 1);
		
		for (cur = ih - 1; cur >= end; cur--)
			ih_dec_offset(cur, len);

		/* Updating node header and item header */
		nh_inc_free_space(node, len);
	}

	nh_dec_free_space_start(node, len);
	node->dirty = 1;
	
	return 0;
}

/* Makes copy of @count items from @src_entity to @dst_entity */
errno_t node_large_rep(object_entity_t *dst_entity, pos_t *dst_pos,
		       object_entity_t *src_entity, pos_t *src_pos,
		       uint32_t count)
{
	uint32_t size;
	uint32_t items;
	uint32_t fss, i;
	uint32_t offset;
	uint32_t headers;

	node_t *dst_node;
	node_t *src_node;
	item_header_t *ih;
	item_header_t *end;
	void *src, *dst, *body;

	dst_node = (node_t *)dst_entity;
	src_node = (node_t *)src_entity;

	items = nh_get_num_items(dst_node);
	headers = count * sizeof(item_header_t);
	fss = nh_get_free_space_start(dst_node);
	
	if (!(size = node_large_size(src_node, src_pos, count)))
		return -EINVAL;
	
	/* Copying item bodies from src node to dst one */
	src = node_large_ib_at(src_node, src_pos->item);

	if (dst_pos->item < items - count)
		body = node_large_ib_at(dst_node, dst_pos->item);
	else
		body = dst_node->block->data + fss - size;
		
	aal_memcpy(body, src, size);

	/* Copying item headers from src node to dst one */
	src = node_large_ih_at(src_node, src_pos->item +
			   count - 1);

	dst = node_large_ih_at(dst_node, dst_pos->item +
			   count - 1);
			
	aal_memcpy(dst, src, headers);

	/* Updating item headers in dst node */
	end = node_large_ih_at(dst_node, items - 1);
	ih = (item_header_t *)dst + count - 1;
	
	offset = (body - dst_node->block->data);
	
	for (i = 0; i < count; i++, ih--) {
		uint32_t old = ih_get_offset(ih);
		
		ih_set_offset(ih, offset);
		
		if (ih == end)
			offset += fss - ih_get_offset(ih);
		else
			offset += ih_get_offset(ih - 1) - old;
	}
	
	dst_node->dirty = 1;
	return 0;
}

/* Inserts item described by hint structure into node */
static errno_t node_large_insert(object_entity_t *entity, pos_t *pos,
				 create_hint_t *hint)
{
	errno_t res;
	node_t *node;
	place_t place;
	item_header_t *ih;
    
	aal_assert("vpf-119", pos != NULL);
	aal_assert("umka-1814", hint != NULL);

	aal_assert("umka-818", entity != NULL);
	aal_assert("umka-2026", loaded(entity));
    
	/* Makes expand of the node new items will be inserted in */
	if (node_large_expand(entity, pos, hint->len, 1)) {
		aal_exception_error("Can't expand node for insert "
				    "item/unit.");
		return -EINVAL;
	}

	node = (node_t *)entity;
	ih = node_large_ih_at(node, pos->item);

	/* Updating item header if we want insert new item */
	if (pos->unit == MAX_UINT32) {
		ih_set_pid(ih, hint->plug->id.id);

		aal_memcpy(&ih->key, hint->key.body,
			   sizeof(ih->key));
	}
	
	/* Preparing item for calling item plugin with them */
	if (node_large_get_item(entity, pos, &place)) {
		aal_exception_error("Can't fetch item data.");
		return -EINVAL;
	}

	/* Updating item header plugin id if we insert new item */
	if (pos->unit == MAX_UINT32) {
		if (hint->flags == HF_RAWDATA) {
			aal_memcpy(place.body, hint->type_specific,
				   hint->len);

			node->dirty = 1;
			return 0;
		}

		/* Calling item plugin to perform initializing the item */
		if (hint->plug->o.item_ops->init)
			hint->plug->o.item_ops->init(&place);

		/* Inserting units into @item */
		if ((res = plug_call(hint->plug->o.item_ops,
				     insert, &place, hint, 0)))
		{
			aal_exception_error("Can't create new item in "
					    "node %llu.", node->number);
			return res;
		}
	} else {
		/* Inserting units into @place */
		if ((res = plug_call(hint->plug->o.item_ops,
				     insert, &place, hint, pos->unit)))
		{
			aal_exception_error("Can't insert unit to "
					    "node %llu.", node->number);
			return res;
		}
	}
	
	/* Updating item's key if we insert new item or if we insert unit into
	   leftmost postion. */
	if (pos->unit == 0) {
		aal_memcpy(&ih->key, place.key.body, sizeof(ih->key));
	}

	return 0;
}

/* This function removes item/unit from the node at specified @pos */
errno_t node_large_remove(object_entity_t *entity, 
			  pos_t *pos, uint32_t count) 
{
	pos_t rpos;
	uint32_t len;
	node_t *node;
	place_t place;
	
	aal_assert("umka-987", pos != NULL);
	aal_assert("umka-986", entity != NULL);
	aal_assert("umka-2027", loaded(entity));

	node = (node_t *)entity;

	if (node_large_get_item(entity, pos, &place))
		return -EINVAL;

	rpos = *pos;

	/* Checking if we need remove whole item if it has not units anymore */
	if (plug_call(place.plug->o.item_ops, units, &place) == 1)
		rpos.unit = MAX_UINT32;
	
	if (rpos.unit == MAX_UINT32) {
		if (!(len = node_large_size(node, &rpos, count)))
			return -EINVAL;
	} else {
		/* Removing units from the item pointed by @pos */
		len = plug_call(place.plug->o.item_ops, remove,
				&place, rpos.unit, count);

                /* Updating items key if leftmost unit was changed */
		if (rpos.unit == 0) {
			item_header_t *ih = node_large_ih_at(node, rpos.item);
			aal_memcpy(&ih->key, place.key.body, sizeof(ih->key));
		}
	}
	
	return node_large_shrink(entity, &rpos, len, count);
}

/* Removes items/units starting from the @start and ending at the @end */
static errno_t node_large_cut(object_entity_t *entity,
			      pos_t *start, pos_t *end)
{
	pos_t pos;

	node_t *node;
	place_t place;

	uint32_t units;
	uint32_t begin;
	uint32_t count;
	
	aal_assert("umka-1790", end != NULL);
	aal_assert("umka-1789", start != NULL);
	aal_assert("umka-1788", entity != NULL);
	aal_assert("umka-2028", loaded(entity));

	node = (node_t *)entity;
		
	/* Check if there some amount of whole items can be removed */
	if (start->item != end->item) {

		begin = start->item + 1;
		count = end->item - start->item;
		
		/* Removing units inside start item */
		if (start->unit != MAX_UINT32) {
			pos = *start;
			
			if (node_large_get_item(entity, &pos, &place))
				return -EINVAL;
				
			units = plug_call(place.plug->o.item_ops,
					  units, &place);

			if (node_large_remove(entity, &pos, units - start->unit))
				return -EINVAL;
			
			if (start->unit == 0)
				begin--;
		}

		/* Removing units inside end item */
		if (end->unit != MAX_UINT32) {
			pos = *end;
			
			if (node_large_get_item(entity, &pos, &place))
				return -EINVAL;
				
			units = plug_call(place.plug->o.item_ops,
					  units, &place);

			if (node_large_remove(entity, &pos, end->unit))
				return -EINVAL;
			
			if (end->unit >= units)
				count++;
		}
			
		if (count > 0) {
                        /* Removing some amount of whole items from the node. If
			   previous node_large_remove produced empty edge items,
			   they will be removed too. */
			POS_INIT(&pos, begin, MAX_UINT32);
			
			if (node_large_remove(entity, &pos, count))
				return -EINVAL;
		}
	} else {
		aal_assert("umka-1795", end->unit != MAX_UINT32);
		aal_assert("umka-1794", start->unit != MAX_UINT32);
		
		pos = *start;
		count = end->unit - start->unit;
		
		if (node_large_remove(entity, &pos, count))
			return -EINVAL;

		if (node_large_get_item(entity, &pos, &place))
			return -EINVAL;

		/* Remove empty item */
		if (!(units = plug_call(place.plug->o.item_ops,
					units, &place)))
		{
			pos.unit = MAX_UINT32;

			if (node_large_shrink(entity, &pos, place.len, 1))
				return -EINVAL;
		}
	}

	return 0;
}

/* Updates key at @pos by specified @key */
static errno_t node_large_set_key(object_entity_t *entity, 
				  pos_t *pos, key_entity_t *key) 
{
	node_t *node;
	item_header_t *ih;

	aal_assert("umka-819", key != NULL);
    	aal_assert("umka-944", pos != NULL);
	
	aal_assert("umka-811", pos->item <
		   node_common_items(entity));

	aal_assert("umka-809", entity != NULL);
	aal_assert("umka-2041", loaded(entity));

	node = (node_t *)entity;
	
	ih = node_large_ih_at(node, pos->item);
	aal_memcpy(&ih->key, key->body, sizeof(ih->key));

	node->dirty = 1;
	return 0;
}

/* Prepare text node description and push it into specified @stream. */
static errno_t node_large_print(object_entity_t *entity,
				aal_stream_t *stream,
				uint32_t start, 
				uint32_t count, 
				uint16_t options) 
{
	pos_t pos;
	node_t *node;
	place_t place;

	uint8_t level;
	uint32_t last;

	aal_assert("vpf-023", entity != NULL);
	aal_assert("umka-457", stream != NULL);
	aal_assert("umka-2044", loaded(entity));

	node = (node_t *)entity;
	level = node_common_get_level(entity);
	
	aal_assert("umka-1580", level > 0);

	aal_stream_format(stream, "NODE (%llu) LEVEL=%u ITEMS=%u "
			  "SPACE=%u MKFS=0x%llx FLUSH=0x%llx\n",
			  aal_block_number(node->block), level,
			  node_common_items(entity), node_common_space(entity),
			  nh_get_mkfs_id(node), nh_get_flush_id(node));
	
	pos.unit = MAX_UINT32;
	
	if (start == MAX_UINT32)
		start = 0;
	
	last = node_common_items(entity);
	
	if (last > start + count)
		last = start + count;
	
	/* Loop through the all items */
	for (pos.item = start; pos.item < last; pos.item++) {

		if (node_large_get_item(entity, &pos, &place))
			return -EINVAL;

		aal_stream_format(stream, "(%u) ", pos.item);
		
		/* Printing item by means of calling item print method */
		if (place.plug->o.item_ops->print) {
			if (place.plug->o.item_ops->print(&place, stream, options))
				return -EINVAL;
		} else {
			aal_stream_format(stream, "Method \"print\" is "
					  "not implemented for \"%s\".",
					  place.plug->label);
		}

	}

	aal_stream_format(stream, "\n");
	return 0;
}
#endif

/* Helper callback for comparing two keys. This is used by node lookup */
static int callback_comp_key(void *node, uint32_t pos,
			     body_t *key2, void *data)
{
	body_t *key1;
	
	aal_assert("umka-566", node != NULL);
	aal_assert("umka-567", key2 != NULL);
	aal_assert("umka-656", data != NULL);

	key1 = &node_large_ih_at((node_t *)node, pos)->key;

	return plug_call(((reiser4_plug_t *)data)->o.key_ops,
			 compraw, key1, key2);
}

/* Makes search inside the specified node @entity for @key and stores the result
   into @pos. This function returns 1 if key is found and 0 otherwise. */
static lookup_t node_large_lookup(object_entity_t *entity, 
				  key_entity_t *key, pos_t *pos)
{
	aal_assert("umka-472", key != NULL);
	aal_assert("umka-478", pos != NULL);
	aal_assert("umka-470", entity != NULL);
	aal_assert("umka-2046", loaded(entity));
	aal_assert("umka-714", key->plug != NULL);

	switch (aux_bin_search(entity, node_common_items(entity),
			       key->body, callback_comp_key,
			       key->plug, &pos->item))
	{
	case 1:
		return PRESENT;
	case 0:
		return ABSENT;
	default:
		return FAILED;
	}
}

#ifndef ENABLE_STAND_ALONE
/* Checks if two item entities are mergeable */
static bool_t node_large_mergeable(place_t *src, place_t *dst) {
	if (!plug_equal(src->plug, dst->plug))
		return FALSE;

	if (!src->plug->o.item_ops->mergeable)
		return FALSE;
	
	return src->plug->o.item_ops->mergeable(src, dst);
}

static bool_t node_large_splitable(place_t *place) {
	if (!place->plug->o.item_ops->shift)
		return FALSE;
	
	if (!place->plug->o.item_ops->estimate_shift)
		return FALSE;
	
	/* We can't shift units from items with one unit */
	if (!place->plug->o.item_ops->units)
		return FALSE;

	/* Items that consist of one unit cannot be splitted */
	if (place->plug->o.item_ops->units(place) <= 1)
		return FALSE;
	
	return TRUE;
}

/* Fuses two items is they are mergeable */
static errno_t node_large_fuse(object_entity_t *src_entity,
			       pos_t *src_pos,
			       object_entity_t *dst_entity,
			       pos_t *dst_pos)
{
	pos_t pos;
	void *body;
	errno_t res;
	uint32_t len;

	place_t src_place;
	place_t dst_place;

	uint32_t src_units;
	uint32_t dst_units;
	
	aal_assert("umka-2227", src_pos != NULL);
	aal_assert("umka-2228", src_pos != NULL);
	aal_assert("umka-2225", src_entity != NULL);
	aal_assert("umka-2226", dst_entity != NULL);

	if (aal_abs(src_pos->item - dst_pos->item) > 1)
		return -EINVAL;
	
	/* Initializing items */
	if (node_large_get_item(src_entity, src_pos, &src_place))
		return -EINVAL;
	
	if (node_large_get_item(dst_entity, dst_pos, &dst_place))
		return -EINVAL;

	/* Making copy of the src_item */
	if (!(body = aal_calloc(sizeof(src_place.len), 0)))
		return -ENOMEM;

	aal_memcpy(body, src_place.body, src_place.len);
	
	src_place.body = body;
	
	/* Removing src item from the node */
	if ((res = node_large_shrink(src_entity, src_pos,
				     src_place.len, 1)))
	{
		goto error_free_body;
	}

	/* Expanding node in order to prepare room for new units */
	len = src_place.len;

	if (src_place.plug->o.item_ops->overhead) {
		len -= plug_call(src_place.plug->o.item_ops,
				 overhead, &src_place);
	}
	
	POS_INIT(&pos, dst_pos->item, 0);
	
	if (src_pos->item < dst_pos->item)
		pos.item--;

	/* Reinitializing @dst_item after shrink and pos correcting */
	if ((res = node_large_get_item(dst_entity, &pos, &dst_place)))
		goto error_free_body;
	
	if ((res = node_large_expand(dst_entity, &pos, len, 1))) {
		aal_exception_error("Can't expand item for "
				    "shifting units into it.");
		goto error_free_body;
	}

	/* Copying units @src_item to @dst_item */
	src_units = plug_call(src_place.plug->o.item_ops,
			      units, &src_place);

	if (src_pos->item < dst_pos->item) {
		res = plug_call(src_place.plug->o.item_ops,
				rep, &dst_place, 0, &src_place,
				0, src_units);
	} else {
		dst_units = plug_call(dst_place.plug->o.item_ops,
				      units, &dst_place);
		
		res = plug_call(src_place.plug->o.item_ops,
				rep, &dst_place, dst_units,
				&src_place, 0, src_units);
	}

 error_free_body:
	aal_free(src_place.body);
	return res;
}

/* Merges border items of the src and dst nodes. The behavior depends on the
   passed hint pointer. */
static errno_t node_large_merge(object_entity_t *src_entity,
				object_entity_t *dst_entity, 
				shift_hint_t *hint)
{
	pos_t pos;
	int remove;
	uint32_t len;

	uint32_t overhead;
	uint32_t dst_items;
	uint32_t src_items;
	
	node_t *src_node;
	node_t *dst_node;

	place_t src_place;
	place_t dst_place;

	item_header_t *ih;

	aal_assert("umka-1624", hint != NULL);
	aal_assert("umka-1622", src_entity != NULL);
	aal_assert("umka-1623", dst_entity != NULL);
	
	src_node = (node_t *)src_entity;
	dst_node = (node_t *)dst_entity;
	
	src_items = node_common_items(src_entity);
	dst_items = node_common_items(dst_entity);

	if (src_items == 0 || hint->rest == 0)
		return 0;
	
	/* We can't split the first and last items if they lie in position
	   insert point points to. */
	if (hint->control & SF_LEFT) {
 		if (hint->pos.item == 0 &&
		    hint->pos.unit == MAX_UINT32)
		{
			return 0;
		}
	} else {
		if (hint->pos.item == src_items &&
		    hint->pos.unit == MAX_UINT32)
		{
			return 0;
		}
	}

	/* Initializing items to be examined by the estimate_shift() method of
	   corresponding item plugin. */
	POS_INIT(&pos, (hint->control & SF_LEFT ? 0 :
			src_items - 1), MAX_UINT32);
	
	if (node_large_get_item(src_entity, &pos, &src_place))
		return -EINVAL;

	/* Items that do not implement predict and shift methods cannot be
	   splitted. */
	if (!node_large_splitable(&src_place))
		return 0;
	
	/* Checking if items are mergeable */
	if (dst_items > 0) {
		POS_INIT(&pos, (hint->control & SF_LEFT ?
				dst_items - 1 : 0), MAX_UINT32);
		
		if (node_large_get_item(dst_entity, &pos, &dst_place))
			return -EINVAL;

		if (hint->control & SF_LEFT) {
			hint->create = !node_large_mergeable(&dst_place,
							     &src_place);
		} else {
			hint->create = !node_large_mergeable(&src_place,
							     &dst_place);
		}
	} else
		hint->create = 1;

	/* Calling item's predict() method in order to estimate how many units
	   may be shifted out. This method also updates unit component of insert
	   point position. After this function is finish @hint->rest will
	   contain real number of bytes to be shifted into neighbour item. */
	if (hint->create) {
		uint32_t overhead;

		/* If items are not mergeable and we are in merge mode, we
		   will not create new item in dst node. This mode is needed for
		   mergeing mergeable items when they lie in the same node. */
		if (hint->control & SF_MERGE)
			return 0;
		
		overhead = node_large_overhead(dst_entity);
		
		/* In the case items are not mergeable, we need count also item
		   overhead, because new item will be created. */
		if (hint->rest < overhead)
			return 0;

		hint->rest -= overhead;

		if (plug_call(src_place.plug->o.item_ops,
			      estimate_shift, &src_place,
			      NULL, hint))
		{
			return -EINVAL;
		}

		/* Updating item component of the insert point if it was moved
		   into neighbour item. In the case of creating new item and
		   left merge item pos will be equal to dst_items. */
		if (hint->control & SF_UPTIP && hint->result & SF_MOVIP) {
			hint->pos.item = (hint->control & SF_LEFT ?
					  dst_items : 0);
		}
	} else {
		if (plug_call(src_place.plug->o.item_ops,
			      estimate_shift, &src_place,
			      &dst_place, hint))
		{
			return -EINVAL;
		}

		if (hint->control & SF_UPTIP && hint->result & SF_MOVIP) {
			hint->pos.item = (hint->control & SF_LEFT ?
					  dst_items - 1 : 0);
		}
	}

	/* Units shift code starting here */
	if (hint->units == 0)
		return 0;
	
	if (hint->create) {
		
		/* Expanding dst node with creating new item */
		POS_INIT(&pos, (hint->control & SF_LEFT ?
				dst_items : 0), MAX_UINT32);
		
		if (node_large_expand(dst_entity, &pos, hint->rest, 1)) {
			aal_exception_error("Can't expand node for "
					    "shifting units into it.");
			return -EINVAL;
		}

		hint->items++;
		
		/* Setting up new item fields */
		ih = node_large_ih_at(dst_node, pos.item);
		ih_set_pid(ih, src_place.plug->id.id);

		aal_memcpy(&ih->key, src_place.key.body,
			   sizeof(ih->key));

		/* Initializing dst item after it was created by node_large_expand()
		   function. */
		if (node_large_get_item(dst_entity, &pos, &dst_place))
			return -EINVAL;

		if (dst_place.plug->o.item_ops->init)
			dst_place.plug->o.item_ops->init(&dst_place);

		/* Setting item len to old len, that is zero, as it was just
		   created. This is needed for correct work of shift() method of
		   some items, which do not have "units" field and calculate the
		   number of units by own len, like extent40 does. This is
		   because, extent40 has all units of the same length. */
		dst_place.len = 0;
	} else {
		/* Items are mergeable, so we do not need to create new item in
		   the dst node. We just need to expand existent dst item by
		   hint->rest. So, we will call node_large_expand() with unit
		   component not equal MAX_UINT32. */
		POS_INIT(&pos, (hint->control & SF_LEFT ?
				dst_items - 1 : 0), 0);

		if (node_large_expand(dst_entity, &pos, hint->rest, 1)) {
			aal_exception_error("Can't expand item for "
					    "shifting units into it.");
			return -EINVAL;
		}
	}

	overhead = 0;

	if (src_place.plug->o.item_ops->overhead) {
		overhead = plug_call(src_place.plug->o.item_ops,
				     overhead, &src_place);
	}

	/* As @hint->rest is number of bytes units occupy, we decrease it by
	   item overhead. */
	if (hint->create)
		hint->rest -= overhead;
	
	/* Shift units from @src_item to @dst_item */
	if (plug_call(src_place.plug->o.item_ops, shift,
		      &src_place, &dst_place, hint))
	{
		return -EINVAL;
	}

	/* Updating source node fields */
	pos.item = src_place.pos.item;

	/* We will remove src_item if it has became empty and insert point is
	   not points it. */
	remove = (hint->rest == (src_place.len - overhead) &&
		  (hint->result & SF_MOVIP || pos.item != hint->pos.item));
	
	/* Updating item's keys */
	if (hint->control & SF_LEFT) {
		/* We do not need update key of the src item which is going to
		   be removed. */
		if (!remove) {
			ih = node_large_ih_at(src_node, src_place.pos.item);
			aal_memcpy(&ih->key, src_place.key.body, sizeof(ih->key));
		}
	} else {
		ih = node_large_ih_at(dst_node, dst_place.pos.item);
		aal_memcpy(&ih->key, dst_place.key.body, sizeof(ih->key));
	}
	
	if (remove) {
		/* Like node_large_expand() does, node_large_shrink() will remove
		   pointed item if unit component is MAX_UINT32 and shrink the item
		   pointed by pos if unit component is not MAX_UINT32. */
		pos.unit = MAX_UINT32;
		len = src_place.len;

		/* As item will be removed, we should update item pos in hint
		   properly. */
		if (hint->control & SF_UPTIP && pos.item < hint->pos.item)
			hint->pos.item--;
	} else {
		pos.unit = 0;
		len = hint->rest;
	}

	return node_large_shrink(src_entity, &pos, len, 1);
}

/* Predicts how many whole item may be shifted from @src_entity to
   @dst_entity. */
static errno_t node_large_predict(object_entity_t *src_entity,
				  object_entity_t *dst_entity, 
				  shift_hint_t *hint)
{
	uint32_t flags;
	uint32_t space;
	
	uint32_t src_items;
	uint32_t dst_items;

	node_t *src_node;
	node_t *dst_node;

	item_header_t *cur;
	item_header_t *end;
	
	src_node = (node_t *)src_entity;
	dst_node = (node_t *)dst_entity;

	dst_items = node_common_items(dst_entity);
	
	if (!(src_items = node_common_items(src_entity)))
		return 0;

	space = node_common_space(dst_entity);

	end = node_large_ih_at(src_node,
			   src_items - 1);
	
	cur = (hint->control & SF_LEFT ?
	       node_large_ih_at(src_node, 0) : end);
	
	/* Estimating will be finished if @src_items value is exhausted or
	   insert point is shifted out to neighbour node. */
	flags = hint->control;
	
	while (!(hint->result & SF_MOVIP) && src_items > 0) {
		uint32_t len;

		if (!(flags & SF_MOVIP) && (flags & SF_RIGHT)) {
			if (hint->pos.item >= src_items)
				break;
		}
		
		/* Getting length of current item */
		len = (cur == end ? nh_get_free_space_start(src_node) :
		       ih_get_offset(cur - 1)) - ih_get_offset(cur);
		
		/* We go out if there is no enough free space to shift one more
		   whole item. */
		if (space < len + node_large_overhead(dst_entity))
			break;

		if (flags & SF_UPTIP) {
			
			/* Updating insert position */
			if (flags & SF_LEFT) {
				if (hint->pos.item == 0) {
					pos_t pos;
					place_t place;
					uint32_t units;

					/* If unit component if zero, we can
					   shift whole item pointed by pos. */
					POS_INIT(&pos, 0, MAX_UINT32);
					
					if (node_large_get_item(src_entity,
								&pos, &place))
					{
						return -EINVAL;
					}

					if (!place.plug->o.item_ops->units)
						return -EINVAL;
				
					units = place.plug->o.item_ops->units(&place);

					/* Breaking if insert point reach the
					   end of node. */
					if (flags & SF_MOVIP &&
					    (hint->pos.unit == MAX_UINT32 ||
					     hint->pos.unit >= units - 1))
					{
						/* If we are permitted to move
						   insetr point to the neigbour,
						   we doing it. */
						hint->result |= SF_MOVIP;
						hint->pos.item = dst_items;
					} else
						break;
				
				} else
					hint->pos.item--;
			} else {
				/* Checking if insert point reach the end of
				   node. Hint is updated here. */
				if (hint->pos.item >= src_items - 1) {
				
					if (hint->pos.item == src_items - 1) {

						if (flags & SF_MOVIP &&
						    (hint->pos.unit == MAX_UINT32 ||
						     hint->pos.unit == 0))
						{
							hint->result |= SF_MOVIP;
							hint->pos.item = 0;
						} else {
							if (hint->pos.unit != MAX_UINT32)
								break;
						}
					} else {
						/* Insert point at the not
						   existent item at the end of
						   node. So we just update hint
						   and breaking the loop. */
						if (flags & SF_MOVIP) {
							hint->result |= SF_MOVIP;
							hint->pos.item = 0;
						}
						break;
					}
				}
			}
		}

		/* Updating some counters and shift hint */
		hint->items++;
		hint->bytes += len;
		src_items--; dst_items++;
		
		cur += (flags & SF_LEFT ? -1 : 1);
		space -= (len + node_large_overhead(dst_entity));
	}

	/* After number of whole items was estimated, all free space will be
	   used for estimating how many units may be shifted. */
	hint->rest = space;
	return 0;
}

/* Moves some amount of whole items from @src_entity to @dst_entity */
static errno_t node_large_transfuse(object_entity_t *src_entity,
				    object_entity_t *dst_entity, 
				    shift_hint_t *hint)
{	
	errno_t res;
	pos_t src_pos;
	pos_t dst_pos;
	
	uint32_t src_items;
	uint32_t dst_items;

	aal_assert("umka-1620", hint != NULL);
	aal_assert("umka-1621", src_entity != NULL);
	aal_assert("umka-1619", dst_entity != NULL);

	if ((res = node_large_predict(src_entity, dst_entity, hint)))
		return res;

	/* No items to be shifted */
	if (hint->items == 0 || hint->bytes == 0)
		return 0;
	
	dst_items = node_common_items(dst_entity);
	src_items = node_common_items(src_entity);

	if (hint->control & SF_LEFT) {
		POS_INIT(&src_pos, 0, MAX_UINT32);
		POS_INIT(&dst_pos, dst_items, MAX_UINT32);
	} else {
		POS_INIT(&dst_pos, 0, MAX_UINT32);

		POS_INIT(&src_pos, src_items - hint->items,
			 MAX_UINT32);
	}
	
	/* Expanding dst node in order to making room for new items and update
	   node header. */
	if (node_large_expand(dst_entity, &dst_pos,
			      hint->bytes, hint->items))
	{
		return -EINVAL;
	}
		
	/* Copying items from src node to dst one */
	if (node_large_rep(dst_entity, &dst_pos, src_entity,
			   &src_pos, hint->items))
	{
		return -EINVAL;
	}

	/* Shrinking source node after items are copied from it to dst node. */
	if (node_large_shrink(src_entity, &src_pos, hint->bytes,
			      hint->items))
	{
		return -EINVAL;
	}
	
	return 0;
}

/* Performs shift of items and units from @entity to @neighb */
static errno_t node_large_shift(object_entity_t *src_entity,
				object_entity_t *dst_entity,
				shift_hint_t *hint)
{
	errno_t res;
	node_t *src_node;
	node_t *dst_node;
	shift_hint_t merge;

	aal_assert("umka-2050", src_entity != NULL);
	aal_assert("umka-2051", dst_entity != NULL);

	aal_assert("umka-2048", loaded(src_entity));
	aal_assert("umka-2049", loaded(dst_entity));

	src_node = (node_t *)src_entity;
	dst_node = (node_t *)dst_entity;

	merge = *hint;
	
	/* First of all we should try to merge boundary items if they are
	   mergeable. This work is performed by unit shift methods with the
	   special shift flags SF_MERGE. It will forbid creating the new item if
	   boundary items are not mergeable. */
	merge.control |= SF_MERGE;
	merge.rest = node_common_space(dst_entity);
	
	/* Merges nodes without ability to create the new item in the
	   @dst_node. This is needed for avoiding the case when a node will
	   contain two neighbour items which are mergeable. That would be not
	   optimal space usage and might also led to some unstable behavior of
	   the code which assume that next mergeable item lies in the neighbour
	   node, not the next to it (directory read and lookup code). */
	if ((res = node_large_merge(src_entity, dst_entity, &merge))) {
		aal_exception_error("Can't merge two nodes durring "
				    "node shift operation.");
		return res;
	}

	hint->pos = merge.pos;
	hint->result = merge.result;

	if (hint->result & SF_MOVIP)
		goto out_update_hint;

	/* Moving some amount of whole items from @src_node to @dst_node */
	if ((res = node_large_transfuse(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't transfuse two nodes "
				    "durring node shift operation.");
		return res;
	}

	/* Checking if insert point was not moved into the corresponding
	   neighbour. */
	if (hint->result & SF_MOVIP)
		goto out_update_hint;

	/* Merges border items with ability to create new item in the dst node.
	   Here our objective is to shift into neighbour node as many units as
	   possible. */
	if ((res = node_large_merge(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't merge two nodes durring "
				    "node shift operation.");
		return res;
	}

	/* The case when insert point is moved to the neighbour node, but
	   nothing was shifted because old insert point was at last item and
	   last unit.  Thus, insert unit request should be converted into insert
	   item one by means of clearing unit component of the insert point in
	   shift hint. */
	if (hint->control & SF_UPTIP &&
	    hint->result & SF_MOVIP &&
	    hint->units == 0 && hint->create)
	{
		hint->pos.unit = MAX_UINT32;
	}

 out_update_hint:
	hint->units += merge.units;
	hint->items += merge.items;
	return 0;
}

extern void node_large_set_flag(object_entity_t *entity, 
				uint32_t pos, 
				uint16_t flag);

extern void node_large_clear_flag(object_entity_t *entity, 
				  uint32_t pos, 
				  uint16_t flag);

extern bool_t node_large_test_flag(object_entity_t *entity, 
				   uint32_t pos, 
				   uint16_t flag);

extern errno_t node_large_copy(object_entity_t *dst_entity,
			       pos_t *dst_pos, 
			       object_entity_t *src_entity,
			       pos_t *src_pos, 
			       copy_hint_t *hint);

extern errno_t node_large_check_struct(object_entity_t *entity,
				       uint8_t mode);

#endif

static reiser4_node_ops_t node_large_ops = {
	.init		= node_large_init,
	.load		= node_common_load,
	.close		= node_common_close,
	.unload		= node_common_unload,	
	.confirm	= node_common_confirm,
	.items		= node_common_items,
	.lookup		= node_large_lookup,
	
	.get_key	= node_large_get_key,
	.get_item       = node_large_get_item,
	.get_level	= node_common_get_level,
		
#ifndef ENABLE_STAND_ALONE
	.get_mstamp	= node_common_get_mstamp,
	.get_fstamp     = node_common_get_fstamp,
		
	.move		= node_common_move,
	.clone          = node_common_clone,
	.form		= node_common_form,
	.sync           = node_common_sync,
	
	.isdirty        = node_common_isdirty,
	.mkdirty        = node_common_mkdirty,
	.mkclean        = node_common_mkclean,
	
	.insert		= node_large_insert,
	.remove		= node_large_remove,
	.cut            = node_large_cut,
	.print		= node_large_print,
	.shift		= node_large_shift,
	.shrink		= node_large_shrink,
	.expand		= node_large_expand,
	.copy           = node_large_copy,
	.rep            = node_large_rep,

	.overhead	= node_large_overhead,
	.maxspace	= node_large_maxspace,
	.space		= node_common_space,
	
	.set_key	= node_large_set_key,
	.set_level      = node_common_set_level,
	.set_mstamp	= node_common_set_mstamp,
	.set_fstamp     = node_common_set_fstamp,
	
	.set_flag	= node_large_set_flag,
	.clear_flag	= node_large_clear_flag,
	.test_flag	= node_large_test_flag,

	.check_struct	= node_large_check_struct
#endif
};

static reiser4_plug_t node_large_plug = {
	.cl    = CLASS_INIT,
	.id    = {NODE_LARGE_ID, 0, NODE_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "node_large",
	.desc  = "Node plugin for reiser4, ver. " VERSION,
#endif
	.o = {
		.node_ops = &node_large_ops
	}
};

static reiser4_plug_t *node_large_start(reiser4_core_t *c) {
	core = c;
	return &node_large_plug;
}

plug_register(node_large, node_large_start, NULL);
#endif
