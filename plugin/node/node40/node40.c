/*
  node40.c -- reiser4 default node plugin.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "node40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t node40_plugin;

#ifndef ENABLE_STAND_ALONE
static int node40_isdirty(object_entity_t *entity) {
	aal_assert("umka-2091", entity != NULL);
	return ((node40_t *)entity)->dirty;
}

static void node40_mkdirty(object_entity_t *entity) {
	aal_assert("umka-2092", entity != NULL);
	((node40_t *)entity)->dirty = 1;
}

static void node40_mkclean(object_entity_t *entity) {
	aal_assert("umka-2093", entity != NULL);
	((node40_t *)entity)->dirty = 0;
}
#endif

/* Returns node level */
static uint8_t node40_get_level(object_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL);
	aal_assert("umka-2017", node40_loaded(entity));
	
	return nh40_get_level((node40_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns node make stamp */
static uint32_t node40_get_mstamp(object_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL);
	aal_assert("umka-2018", node40_loaded(entity));
	
	return nh40_get_mkfs_id((node40_t *)entity);
}

/* Returns node flush stamp */
static uint64_t node40_get_fstamp(object_entity_t *entity) {
	aal_assert("vpf-645", entity != NULL);
	aal_assert("umka-2019", node40_loaded(entity));
	
	return nh40_get_flush_id((node40_t *)entity);
}
#endif

/* Returns item header by pos */
item40_header_t *node40_ih_at(node40_t *node, uint32_t pos) {
	aal_block_t *block;
	item40_header_t *ih;

	block = node->block;

	ih = (item40_header_t *)(block->data +
				 aal_block_size(block));
	
	return ih - pos - 1;
}

/* Retutrns item body by pos */
void *node40_ib_at(node40_t *node, uint32_t pos) {
	aal_block_t *block;
	item40_header_t *ih;
	
	block = node->block;
	ih = node40_ih_at(node, pos);
	
	return block->data + ih40_get_offset(ih);
}

/* Returns node free space end offset */
uint16_t node40_free_space_end(node40_t *node) {
	uint32_t items = nh40_get_num_items(node);

	return aal_block_size(node->block) - items *
		sizeof(item40_header_t);
}

/*
  Creates node40 entity on specified device and block number. This can be used
  later for working with all node methods.
*/
static object_entity_t *node40_init(aal_device_t *device,
				    uint32_t size, blk_t blk)
{
	node40_t *node;
    
	aal_assert("umka-806", device != NULL);

	/* Allocating memory for the entity */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	node->blk = blk;
	
#ifndef ENABLE_STAND_ALONE
	node->dirty = 0;
#endif
	
	node->size = size;
	node->device = device;
	node->plugin = &node40_plugin;

	return (object_entity_t *)node;
}

#ifndef ENABLE_STAND_ALONE
static void node40_move(object_entity_t *entity,
			blk_t number)
{
	node40_t *node;
	
	aal_assert("umka-2249", entity != NULL);
	aal_assert("umka-2012", node40_loaded(entity));

	node = (node40_t *)entity;
	aal_block_move(node->block, number);
}

/* Opens node on passed device and block number */
static errno_t node40_form(object_entity_t *entity,
			   uint8_t level)
{
	uint32_t header_size;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-2013", entity != NULL);

	if (node->block == NULL) {
		if (!(node->block = aal_block_create(node->device,
						     node->size,
						     node->blk, 0)))
		{
			return -ENOMEM;
		}
	}

	nh40_set_num_items(node, 0);
	nh40_set_level(node, level);
	nh40_set_magic(node, NODE40_MAGIC);
	nh40_set_pid(node, NODE_REISER40_ID);

	header_size = sizeof(node40_header_t);
	nh40_set_free_space_start(node, header_size);
	nh40_set_free_space(node, node->size - header_size);

	node->dirty = 1;
	return 0;
}
#endif

static errno_t node40_load(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
	
	aal_assert("umka-2010", entity != NULL);
	
	if (node->block)
		return 0;

	if (!(node->block = aal_block_read(node->device,
					   node->size,
					   node->blk)))
	{
		return -EIO;
	}

#ifndef ENABLE_STAND_ALONE
	node->dirty = 0;
#endif
	
	return 0;
}

static errno_t node40_unload(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
	
	aal_assert("umka-2011", entity != NULL);
	aal_assert("umka-2012", node40_loaded(entity));

	aal_block_free(node->block);
	node->block = NULL;

#ifndef ENABLE_STAND_ALONE
	node->dirty = 0;
#endif

	return 0;
}

/* Closes node by means of closing its block */
static errno_t node40_close(object_entity_t *entity) {
	node40_t *node;

	aal_assert("umka-825", entity != NULL);

	node = (node40_t *)entity;

	if (node->block)
		node40_unload(entity);
	
	aal_free(entity);
	return 0;
}

/* Confirms that passed node corresponds current plugin */
static int node40_confirm(object_entity_t *entity) {
	aal_assert("vpf-014", entity != NULL);
	aal_assert("umka-2020", node40_loaded(entity));
	
	return (nh40_get_magic((node40_t *)entity) == NODE40_MAGIC);
}

/*
  Returns item number in passed node entity. Used for any loops through the all
  node items.
*/
uint16_t node40_items(object_entity_t *entity) {
	aal_assert("vpf-018", entity != NULL);
	aal_assert("umka-2021", node40_loaded(entity));
	
	return nh40_get_num_items((node40_t *)entity);
}

/* Returns key at passed @pos */
static errno_t node40_get_key(object_entity_t *entity,
			      pos_t *pos, key_entity_t *key) 
{
	void *body;
    
	aal_assert("umka-821", key != NULL);
	aal_assert("umka-939", pos != NULL);
	aal_assert("vpf-009", entity != NULL);
	aal_assert("umka-2022", node40_loaded(entity));

	aal_assert("umka-810", pos->item <
		   nh40_get_num_items((node40_t *)entity));

	body = &(node40_ih_at((node40_t *)entity,
			      pos->item)->key);
	
	aal_memcpy(key->body, body, sizeof(key40_t));
    
	return 0;
}

/* Returns length of item at pos. */
static uint16_t node40_item_len(object_entity_t *entity, 
				pos_t *pos)
{
	item40_header_t *ih;
    
	aal_assert("vpf-037", entity != NULL);
	aal_assert("umka-942", pos != NULL);
	aal_assert("umka-2024", node40_loaded(entity));

	/*
	  Item length is calculated as next item body offset minus current item
	  offset. If we are on the last item then we use free space start for
	  that. We use this way, because reiser4 kernel code does not set item's
	  length correctly. And they are rather reserved for future using.
	*/
	ih = node40_ih_at((node40_t *)entity, pos->item);

	if (pos->item == (uint32_t)(node40_items(entity) - 1)) {
		return nh40_get_free_space_start((node40_t *)entity) -
			ih40_get_offset(ih);
	}

	return ih40_get_offset(ih - 1) - ih40_get_offset(ih);
}

/*
  Initializes item entity in order to pass it to an item plugin routine. If unit
  component of pos is set up the function will initialize item's key from the
  unit one.
*/
static errno_t node40_get_item(object_entity_t *entity,
			       pos_t *pos, item_entity_t *item)
{
	rid_t pid;
	node40_t *node;
	
	aal_assert("umka-1813", pos != NULL);
	aal_assert("umka-1602", item != NULL);
	aal_assert("umka-1631", entity != NULL);

	node = (node40_t *)entity;
	
	aal_assert("umka-1812", pos->item <
		   nh40_get_num_items(node));
	
	/* Initializes item's context (device, block number, etc) */
	item->context.blocksize = node->size;
	item->context.device = node->block->device;
	item->context.blk = aal_block_number(node->block);

	/* Initializing item's plugin */
	pid = ih40_get_pid(node40_ih_at(node, pos->item));
	
	if (!(item->plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id "
				    "0x%x", pid);
		return -EINVAL;
	}

	/* Initializing item's pos, body pointer and length */
	item->len = node40_item_len(entity, pos);
	item->body = node40_ib_at(node, pos->item);
	aal_memcpy(&item->pos, pos, sizeof(pos_t));

	return 0;
}

#ifndef ENABLE_STAND_ALONE
errno_t node40_item(object_entity_t *entity, pos_t *pos,
		    item_entity_t *item)
{
	errno_t res;
	node40_t *node;
	
	node = (node40_t *)entity;
	
	if ((res = node40_get_item(entity, pos, item)))
		return res;
	
	if (!(item->key.plugin = core->factory_ops.ifind(KEY_PLUGIN_TYPE,
							 KEY_REISER40_ID)))
	{
		aal_exception_error("Can't find key plugin by its id "
				    "0x%x", KEY_REISER40_ID);
		return -EINVAL;
	}

	/* Getting item key */
	return node40_get_key(entity, pos, &item->key);
}

/* Returns node free space */
static uint16_t node40_space(object_entity_t *entity) {
	aal_assert("vpf-020", entity != NULL);
	aal_assert("umka-2025", node40_loaded(entity));
	
	return nh40_get_free_space((node40_t *)entity);
}

/*
  Retutns item overhead for this node format. Widely used in modification and
  estimation routines.
*/
static uint16_t node40_overhead(object_entity_t *entity) {
	return sizeof(item40_header_t);
}

/* Returns maximal size of item possible for passed node instance */
static uint16_t node40_maxspace(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-016", node != NULL);

	/*
	  Maximal space is node size minus node header and minus item
	  overhead.
	*/
	return node->size - sizeof(node40_header_t) -
		sizeof(item40_header_t);
}

/*
  Calculates size of a region denoted by @pos and @count. This is used by
  node40_rep(), node40_remove(), etc.
*/
static uint32_t node40_size(node40_t *node, pos_t *pos,
			    uint32_t count)
{
	uint32_t len;
	uint32_t items;
	
	item40_header_t *cur;

	items = nh40_get_num_items(node);

	aal_assert("umka-1811", pos->item +
		   count <= items);
	
	cur = node40_ih_at(node, pos->item);

	if (pos->item + count < items)
		len = ih40_get_offset(cur - count);
	else
		len = nh40_get_free_space_start(node);

	return len - ih40_get_offset(cur);
}

/*
  Makes expand passed @node by @len in odrer to make room for insert new
  items/units. This function is used by insert and shift methods.
*/
errno_t node40_expand(object_entity_t *entity, pos_t *pos,
		      uint32_t len, uint32_t count)
{
	int is_insert;

	uint32_t item;
	node40_t *node;
	uint32_t items;
	uint32_t offset;
	uint32_t headers;
	item40_header_t *ih;

	aal_assert("vpf-006", pos != NULL);
	aal_assert("umka-817", entity != NULL);

	node = (node40_t *)entity;

	if (len == 0)
		return 0;
	
	/* Checks for input validness */
	is_insert = (pos->unit == ~0ul);

	items = nh40_get_num_items(node);
	headers = count * sizeof(item40_header_t);

	aal_assert("vpf-026", nh40_get_free_space(node) >= 
		   len + (is_insert ? sizeof(item40_header_t) : 0));
	
	aal_assert("vpf-027", pos->item <= items);

	/* Getting real pos of the item to be updated */
	item = pos->item + !is_insert;
	ih = node40_ih_at(node, item);

	/*
	  If item pos is inside the range [0..count - 1], we should perform the
	  data moving and offset upadting.
	*/
	if (item < items) {
		void *src, *dst;
		uint32_t i, size;

		offset = ih40_get_offset(ih);

		/* Moving items bodies */
		src = node->block->data + offset;
		dst = src + len;

		size = nh40_get_free_space_start(node) - offset;

		aal_memmove(dst, src, size);

		/* Updating item offsets */
		for (i = 0; i < items - item; i++, ih--) 
			ih40_inc_offset(ih, len);

		/*
		  If this is the insert new item mode, we should prepare the
		  room for new item header and set it up.
		*/
		if (is_insert) {
			src = node40_ih_at(node, items - 1);

			dst = node40_ih_at(node, items - 1 +
					   count);

			size = sizeof(item40_header_t) *
				(items - item);
			
			aal_memmove(dst, src, size);
		}

		ih = node40_ih_at(node, item);
	} else
		offset = nh40_get_free_space_start(node);

	/* Updating node's free space and free space start fields */
	nh40_inc_free_space_start(node, len);
	nh40_dec_free_space(node, len);

	if (is_insert) {
                /* Setting up the fields of new item */
		ih40_set_len(ih, len);
		ih40_set_offset(ih, offset);

		/* Setting up node header */
		nh40_inc_num_items(node, count);
		nh40_dec_free_space(node, headers);
	} else {
		/* Increasing item len mfor the case of pasting new units */
		ih = node40_ih_at(node, pos->item);
		ih40_inc_len(ih, len);
	}
	
	node->dirty = 1;
	return 0;
}

/*
  General node40 cutting function. It is used from shift, remove, etc. It
  removes an amount of items specified by @count and shrinks node. 
*/
errno_t node40_shrink(object_entity_t *entity, pos_t *pos,
		      uint32_t len, uint32_t count)
{
	int is_range;
	uint32_t size;
	void *src, *dst;

	node40_t *node;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, items;

	item40_header_t *cur;
	item40_header_t *end;

	aal_assert("umka-1800", count > 0);
	aal_assert("umka-1799", pos != NULL);
	aal_assert("umka-1798", entity != NULL);

	node = (node40_t *)entity;
	items = nh40_get_num_items(node);

	is_range = (pos->item < items);
	aal_assert("umka-1801", is_range);

	if (pos->unit == ~0ul) {
		is_range = (is_range && pos->item + count <= items);
		aal_assert("umka-1802", is_range);

		end = node40_ih_at(node, items - 1);
		headers = count * sizeof(item40_header_t);

		/* Moving item header and bodies if it is needed */
		if (pos->item + count < items) {

			/* Moving item bodies */
			dst = node40_ib_at(node, pos->item);
			src = node40_ib_at(node, pos->item + count);

			size = node40_size(node, pos, items -
					   pos->item) - len;
 
			aal_memmove(dst, src, size);

			/* Moving item headers */
			src = node40_ih_at(node, items - 1);
			dst = src + headers;
			
			size = (items - (pos->item + count)) *
				sizeof(item40_header_t);
	
			aal_memmove(dst, src, size);

			/* Updating item offsets */
			cur = node40_ih_at(node, pos->item);
	
			for (i = pos->item; i < items - count; i++, cur--)
				ih40_dec_offset(cur, len);
		}

		/* Updating node header */
		nh40_dec_num_items(node, count);
		nh40_inc_free_space(node, (len + headers));
	} else {
		uint32_t item_len;
		item40_header_t *ih;
		object_entity_t *entity;

		entity = (object_entity_t *)node;

		ih = node40_ih_at(node, pos->item);
		item_len = node40_item_len(entity, pos);
		
		/* Moving item bodies */
		src = node40_ib_at(node, pos->item) + item_len;
		dst = node40_ib_at(node, pos->item) + item_len - len;

		size = nh40_get_free_space_start(node) -
			ih40_get_offset(ih) - item_len;
		
		aal_memmove(dst, src, size);
		
		/* Updating header offsets */
		end = node40_ih_at(node, items - 1);
		
		for (cur = ih - 1; cur >= end; cur--)
			ih40_dec_offset(cur, len);

		/* Updating node header and item header */
		nh40_inc_free_space(node, len);
		ih40_dec_len(ih, len);
	}

	nh40_dec_free_space_start(node, len);
	node->dirty = 1;
	
	return 0;
}

/* Makes copy of @count items from @src_entity to @dst_entity */
errno_t node40_rep(object_entity_t *dst_entity, pos_t *dst_pos,
		   object_entity_t *src_entity, pos_t *src_pos,
		   uint32_t count)
{
	uint32_t size;
	uint32_t items;
	uint32_t fss, i;
	uint32_t offset;
	uint32_t headers;

	node40_t *dst_node;
	node40_t *src_node;
	item40_header_t *ih;
	item40_header_t *end;
	void *src, *dst, *body;

	dst_node = (node40_t *)dst_entity;
	src_node = (node40_t *)src_entity;

	items = nh40_get_num_items(dst_node);
	headers = count * sizeof(item40_header_t);
	fss = nh40_get_free_space_start(dst_node);
	
	if (!(size = node40_size(src_node, src_pos, count)))
		return -EINVAL;
	
	/* Copying item bodies from src node to dst one */
	src = node40_ib_at(src_node, src_pos->item);

	if (dst_pos->item < items - count)
		body = node40_ib_at(dst_node, dst_pos->item);
	else
		body = dst_node->block->data + fss - size;
		
	aal_memcpy(body, src, size);

	/* Copying item headers from src node to dst one */
	src = node40_ih_at(src_node, src_pos->item +
			   count - 1);

	dst = node40_ih_at(dst_node, dst_pos->item +
			   count - 1);
			
	aal_memcpy(dst, src, headers);

	/* Updating item headers in dst node */
	end = node40_ih_at(dst_node, items - 1);
	ih = (item40_header_t *)dst + count - 1;
	
	offset = (body - dst_node->block->data);
	
	for (i = 0; i < count; i++, ih--) {
		uint32_t old = ih40_get_offset(ih);
		
		ih40_set_offset(ih, offset);
		
		if (ih == end)
			offset += fss - ih40_get_offset(ih);
		else
			offset += ih40_get_offset(ih - 1) - old;
	}
	
	dst_node->dirty = 1;
	return 0;
}

/* Inserts item described by hint structure into node */
static errno_t node40_insert(object_entity_t *entity, pos_t *pos,
			     create_hint_t *hint)
{
	errno_t res;
	node40_t *node;
	item_entity_t item;
	item40_header_t *ih;
    
	aal_assert("vpf-119", pos != NULL);
	aal_assert("umka-1814", hint != NULL);

	aal_assert("umka-818", entity != NULL);
	aal_assert("umka-2026", node40_loaded(entity));
    
	/* Makes expand of the node new items will be inserted in */
	if (node40_expand(entity, pos, hint->len, 1))
		return -EINVAL;

	node = (node40_t *)entity;
	ih = node40_ih_at(node, pos->item);

	/* Updating item header if we want insert new item */
	if (pos->unit == ~0ul) {
		ih40_set_pid(ih, hint->plugin->h.id);

		aal_memcpy(&ih->key, hint->key.body,
			   sizeof(ih->key));
	}
	
	/* Preparing item for calling item plugin with them */
	if (node40_item(entity, pos, &item))
		return -EINVAL;

	/* Updating item header plugin id if we insert new item */
	if (pos->unit == ~0ul) {
		if (hint->flags == HF_RAWDATA) {
			aal_memcpy(item.body, hint->type_specific,
				   hint->len);

			node->dirty = 1;
			return 0;
		}

		/* Calling item plugin to perform initializing the item */
		if (hint->plugin->o.item_ops->init)
			hint->plugin->o.item_ops->init(&item);

		/* Inserting units into @item */
		if ((res = plugin_call(hint->plugin->o.item_ops,
				       insert, &item, hint, 0)))
			return res;
	} else {
		/* Inserting units into @item */
		if ((res = plugin_call(hint->plugin->o.item_ops,
				       insert, &item, hint, pos->unit)))
			return res;
	}
	
	/*
	  Updating item's key if we insert new item or if we insert unit
	  into leftmost postion.
	*/
	if (pos->unit == 0) {
		aal_memcpy(&ih->key, item.key.body,
			   sizeof(ih->key));
	}

	return 0;
}

/* This function removes item/unit from the node at specified @pos */
errno_t node40_remove(object_entity_t *entity, 
		      pos_t *pos, uint32_t count) 
{
	node40_t *node;
	item_entity_t item;
	uint32_t len, units;
	
	aal_assert("umka-987", pos != NULL);
	aal_assert("umka-986", entity != NULL);
	aal_assert("umka-2027", node40_loaded(entity));

	node = (node40_t *)entity;

	if (node40_item(entity, pos, &item))
		return -EINVAL;
	
	units = plugin_call(item.plugin->o.item_ops, units, &item);
	
	if (units == 1)
		pos->unit = ~0ul;
	
	if (pos->unit == ~0ul) {
		if (!(len = node40_size(node, pos, count)))
			return -EINVAL;
	} else {
		/* Removing units from the item pointed by @pos */
		len = plugin_call(item.plugin->o.item_ops, remove, &item,
				  pos->unit, count);

                /* Updating items key if leftmost unit was changed */
		if (pos->unit == 0) {
			item40_header_t *ih = node40_ih_at(node, pos->item);
			aal_memcpy(&ih->key, item.key.body, sizeof(ih->key));
		}
	}
	
	return node40_shrink(entity, pos, len, count);
}

/* Removes items/units starting from the @start and ending at the @end */
static errno_t node40_cut(object_entity_t *entity,
			  pos_t *start, pos_t *end)
{
	pos_t pos;

	node40_t *node;
	uint32_t units;
	
	uint32_t begin;
	uint32_t count;
	
	item_entity_t item;
	
	aal_assert("umka-1790", end != NULL);
	aal_assert("umka-1789", start != NULL);
	aal_assert("umka-1788", entity != NULL);
	aal_assert("umka-2028", node40_loaded(entity));

	node = (node40_t *)entity;
		
	/* Check if there some amount of whole items can be removed */
	if (start->item != end->item) {

		begin = start->item + 1;
		count = end->item - start->item;
		
		/* Removing units inside start item */
		if (start->unit != ~0ul) {
			pos = *start;
			
			if (node40_item(entity, &pos, &item))
				return -EINVAL;
				
			units = plugin_call(item.plugin->o.item_ops,
					    units, &item);

			if (node40_remove(entity, &pos, units - start->unit))
				return -EINVAL;
			
			if (start->unit == 0)
				begin--;
		}

		/* Removing units inside end item */
		if (end->unit != ~0ul) {
			pos = *end;
			
			if (node40_item(entity, &pos, &item))
				return -EINVAL;
				
			units = plugin_call(item.plugin->o.item_ops,
					    units, &item);

			if (node40_remove(entity, &pos, end->unit))
				return -EINVAL;
			
			if (end->unit >= units)
				count++;
		}
			
		if (count > 0) {
                        /*
			  Removing some amount of whole items from the node. If
			  previous node40_remove produced empty edge items, they
			  will be removed too.
			*/
			POS_INIT(&pos, begin, ~0ul);
			
			if (node40_remove(entity, &pos, count))
				return -EINVAL;
		}
	} else {
		aal_assert("umka-1795", end->unit != ~0ul);
		aal_assert("umka-1794", start->unit != ~0ul);
		
		pos = *start;
		count = end->unit - start->unit;
		
		if (node40_remove(entity, &pos, count))
			return -EINVAL;

		if (node40_item(entity, &pos, &item))
			return -EINVAL;

		/* Remove empty item */
		if (!(units = plugin_call(item.plugin->o.item_ops,
					  units, &item)))
		{
			pos.unit = ~0ul;

			if (node40_shrink(entity, &pos, item.len, 1))
				return -EINVAL;
		}
	}

	return 0;
}

/* Returns node make stamp */
static void node40_set_mstamp(object_entity_t *entity,
			      uint32_t stamp)
{
	aal_assert("vpf-644", entity != NULL);
	aal_assert("umka-2038", node40_loaded(entity));
	
	((node40_t *)entity)->dirty = 1;
	nh40_set_mkfs_id((node40_t *)entity, stamp);
}

/* Returns node flush stamp */
static void node40_set_fstamp(object_entity_t *entity,
			      uint64_t stamp)
{
	aal_assert("vpf-643", entity != NULL);
	aal_assert("umka-2039", node40_loaded(entity));
	
	((node40_t *)entity)->dirty = 1;
	nh40_set_flush_id((node40_t *)entity, stamp);
}

static void node40_set_level(object_entity_t *entity,
			     uint8_t level)
{
	aal_assert("umka-1864", entity != NULL);
	aal_assert("umka-2040", node40_loaded(entity));
	
	((node40_t *)entity)->dirty = 1;
	nh40_set_level((node40_t *)entity, level);
}

/* Updates key at @pos by specified @key */
static errno_t node40_set_key(object_entity_t *entity, 
			      pos_t *pos, key_entity_t *key) 
{
	uint32_t items;
	node40_t *node;

	aal_assert("umka-819", key != NULL);
    	aal_assert("umka-944", pos != NULL);
	
	aal_assert("umka-809", entity != NULL);
	aal_assert("umka-2041", node40_loaded(entity));

	node = (node40_t *)entity;
	items = nh40_get_num_items(node);
	
	aal_assert("umka-811", pos->item < items);

	aal_memcpy(&(node40_ih_at(node, pos->item)->key),
		   key->body, sizeof(key->body));

	node->dirty = 1;

	return 0;
}

/* Updating node stamp */
static errno_t node40_set_stamp(object_entity_t *entity,
				uint32_t stamp)
{
	aal_assert("umka-1126", entity != NULL);
	aal_assert("umka-2042", node40_loaded(entity));

	nh40_set_mkfs_id(((node40_t *)entity), stamp);
	return 0;
}

/* Saves node to device */
static errno_t node40_sync(object_entity_t *entity) {
	errno_t res;
	
	aal_assert("umka-1552", entity != NULL);
	aal_assert("umka-2043", node40_loaded(entity));
	
	if ((res = aal_block_write(((node40_t *)entity)->block)))
		return res;

	((node40_t *)entity)->dirty = 0;
	
	return 0;
}

/* Prepare text node description and push it into specified @stream. */
static errno_t node40_print(object_entity_t *entity,
			    aal_stream_t *stream,
			    uint32_t start, 
			    uint32_t count, 
			    uint16_t options) 
{
	pos_t pos;
	uint8_t level;
	uint32_t last;
	node40_t *node;
	item_entity_t item;

	aal_assert("vpf-023", entity != NULL);
	aal_assert("umka-457", stream != NULL);
	aal_assert("umka-2044", node40_loaded(entity));

	node = (node40_t *)entity;
	level = node40_get_level(entity);
	
	aal_assert("umka-1580", level > 0);

	aal_stream_format(stream, "NODE (%llu) LEVEL=%u ITEMS=%u "
			  "SPACE=%u MKFS=0x%llx FLUSH=0x%llx\n",
			  aal_block_number(node->block), level,
			  node40_items(entity), node40_space(entity),
			  nh40_get_mkfs_id(node), nh40_get_flush_id(node));
	
	pos.unit = ~0ul;
	
	if (start == ~0ul)
	    start = 0;
	
	last = node40_items(entity);
	if (last > start + count)
	    last = start + count;
	
	/* Loop through the all items */
	for (pos.item = start; pos.item < last; pos.item++) {

		if (node40_item(entity, &pos, &item))
			return -EINVAL;

		aal_stream_format(stream, "(%u) ", pos.item);
		
		/* Printing item by means of calling item print method */
		if (item.plugin->o.item_ops->print) {
			if (item.plugin->o.item_ops->print(&item, stream, options))
				return -EINVAL;
		} else {
			aal_stream_format(stream, "Method \"print\" is not "
					  "implemented.");
		}

	}

	aal_stream_format(stream, "\n");

	return 0;
}
#endif

/* Helper callback for comparing two keys. This is used by node lookup */
static int callback_comp_key(void *node, uint32_t pos,
			     void *key2, void *data)
{
	void *body;
	key_entity_t key1;
	
	aal_assert("umka-566", node != NULL);
	aal_assert("umka-567", key2 != NULL);
	aal_assert("umka-656", data != NULL);

	/*
	  FIXME-UMKA: Here we should avoid memcpy of the key body in order to
	  keep good performance in tree operations. probably we should introduce
	  new key compare method which operates on memory pointer key body lies
	  in.
	*/
	body = &node40_ih_at((node40_t *)node, pos)->key;
	aal_memcpy(key1.body, body, sizeof(key1.body));

	return plugin_call(((reiser4_plugin_t *)data)->o.key_ops,
			   compare, &key1, key2);
}

/*
  Makes search inside the specified node @entity for @key and stores the result
  into @pos. This function returns 1 if key is found and 0 otherwise.
*/
static lookup_t node40_lookup(object_entity_t *entity, 
			      key_entity_t *key,
			      pos_t *pos)
{
	aal_assert("umka-472", key != NULL);
	aal_assert("umka-478", pos != NULL);
	aal_assert("umka-470", entity != NULL);
	aal_assert("umka-714", key->plugin != NULL);
	aal_assert("umka-2046", node40_loaded(entity));

	switch (aux_bin_search(entity, node40_items(entity),
			       key, callback_comp_key,
			       key->plugin, &pos->item))
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
static bool_t node40_mergeable(item_entity_t *item1,
			       item_entity_t *item2)
{
	if (!plugin_equal(item1->plugin, item2->plugin))
		return FALSE;

	if (!item1->plugin->o.item_ops->mergeable)
		return FALSE;
	
	return item1->plugin->o.item_ops->mergeable(item1, item2);
}

static bool_t node40_splitable(item_entity_t *item) {
	if (!item->plugin->o.item_ops->shift)
		return FALSE;
	
	if (!item->plugin->o.item_ops->estimate_shift)
		return FALSE;
	
	/* We can't shift units from items with one unit */
	if (!item->plugin->o.item_ops->units)
		return FALSE;

	/* Items that consist of one unit cannot be splitted */
	if (item->plugin->o.item_ops->units(item) <= 1)
		return FALSE;
	
	return TRUE;
}

/* Fuses two items is they are mergeable */
static errno_t node40_fuse(object_entity_t *src_entity,
			   pos_t *src_pos,
			   object_entity_t *dst_entity,
			   pos_t *dst_pos)
{
	pos_t pos;
	void *body;
	errno_t res;
	uint32_t len;

	uint32_t src_units;
	uint32_t dst_units;
	
	item_entity_t src_item;
	item_entity_t dst_item;

	aal_assert("umka-2227", src_pos != NULL);
	aal_assert("umka-2228", src_pos != NULL);
	aal_assert("umka-2225", src_entity != NULL);
	aal_assert("umka-2226", dst_entity != NULL);

	if (aal_abs(src_pos->item - dst_pos->item) > 1)
		return -EINVAL;
	
	/* Initializing items */
	if (node40_item(src_entity, src_pos, &src_item))
		return -EINVAL;
	
	if (node40_item(dst_entity, dst_pos, &dst_item))
		return -EINVAL;

	/* Making copy of the src_item */
	if (!(body = aal_calloc(sizeof(src_item.len), 0)))
		return -ENOMEM;

	aal_memcpy(body, src_item.body,
		   src_item.len);
	
	src_item.body = body;
	
	/* Removing src item from the node */
	if ((res = node40_shrink(src_entity, src_pos,
				 src_item.len, 1)))
	{
		goto error_free_body;
	}

	/* Expanding node in order to prepare room for new units */
	len = src_item.len;

	if (src_item.plugin->o.item_ops->overhead) {
		len -= plugin_call(src_item.plugin->o.item_ops,
				   overhead, &src_item);
	}
	
	POS_INIT(&pos, dst_pos->item, 0);
	
	if (src_pos->item < dst_pos->item)
		pos.item--;

	/* Reinitializing @dst_item after shrink and pos correcting */
	if ((res = node40_item(dst_entity, &pos, &dst_item)))
		goto error_free_body;
	
	if ((res = node40_expand(dst_entity, &pos, len, 1))) {
		aal_exception_error("Can't expand item for "
				    "shifting units into it.");
		goto error_free_body;
	}

	/* Copying units @src_item to @dst_item */
	src_units = plugin_call(src_item.plugin->o.item_ops,
				units, &src_item);

	if (src_pos->item < dst_pos->item) {
		res = plugin_call(src_item.plugin->o.item_ops,
				  rep, &dst_item, 0, &src_item,
				  0, src_units);
	} else {
		dst_units = plugin_call(dst_item.plugin->o.item_ops,
					units, &dst_item);
		
		res = plugin_call(src_item.plugin->o.item_ops,
				  rep, &dst_item, dst_units,
				  &src_item, 0, src_units);
	}

 error_free_body:
	aal_free(src_item.body);
	return res;
}

/*
  Merges border items of the src and dst nodes. The behavior depends on the
  passed hint pointer.
*/
static errno_t node40_merge(object_entity_t *src_entity,
			    object_entity_t *dst_entity, 
			    shift_hint_t *hint)
{
	pos_t pos;
	int remove;
	uint32_t len;

	uint32_t overhead;
	uint32_t dst_items;
	uint32_t src_items;
	
	node40_t *src_node;
	node40_t *dst_node;

	item40_header_t *ih;
	
	item_entity_t src_item;
	item_entity_t dst_item;

	aal_assert("umka-1624", hint != NULL);
	aal_assert("umka-1622", src_entity != NULL);
	aal_assert("umka-1623", dst_entity != NULL);
	
	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;
	
	src_items = node40_items(src_entity);
	dst_items = node40_items(dst_entity);

	if (src_items == 0 || hint->rest == 0)
		return 0;
	
	/*
	  We can't split the first and last items if they lie in position insert
	  point points to.
	*/
	if (hint->control & SF_LEFT) {
 		if (hint->pos.item == 0 && hint->pos.unit == ~0ul)
			return 0;
	} else {
		if (hint->pos.item == src_items && hint->pos.unit == ~0ul)
			return 0;
	}

	/*
	  Initializing items to be examined by the estimate_shift() method of
	  corresponding item plugin.
	*/
	POS_INIT(&pos, (hint->control & SF_LEFT ? 0 :
			src_items - 1), ~0ul);
	
	if (node40_item(src_entity, &pos, &src_item))
		return -EINVAL;

	/*
	  Items that do not implement predict and shift methods cannot be
	  splitted.
	*/
	if (!node40_splitable(&src_item))
		return 0;
	
	/* Checking if items are mergeable */
	if (dst_items > 0) {
		POS_INIT(&pos, (hint->control & SF_LEFT ?
				dst_items - 1 : 0), ~0ul);
		
		if (node40_item(dst_entity, &pos, &dst_item))
			return -EINVAL;

		if (hint->control & SF_LEFT)
			hint->create = !node40_mergeable(&dst_item, &src_item);
		else
			hint->create = !node40_mergeable(&src_item, &dst_item);
	} else
		hint->create = 1;

	/*
	  Calling item's "predict" method in order to estimate how many units
	  may be shifted out. This method also updates unit component of insert
	  point position. After this function is finish @hint->rest will contain
	  real number of bytes to be shifted into neighbour item.
	*/
	if (hint->create) {
		uint32_t overhead;

		/*
		  If items are not mergeable and we are in "merge" mode, we will
		  not create new item in dst node. This mode is needed for
		  mergeing mergeable items when they lie in the same node.
		*/
		if (hint->control & SF_MERGE)
			return 0;
		
		overhead = node40_overhead(dst_entity);
		
		/*
		  In the case items are not mergeable, we need count also item
		  overhead, because new item will be created.
		*/
		if (hint->rest < overhead)
			return 0;

		hint->rest -= overhead;

		if (plugin_call(src_item.plugin->o.item_ops,
				estimate_shift, &src_item,
				NULL, hint))
		{
			return -EINVAL;
		}

		/*
		  Updating item component of the insert point if it was moved
		  into neighbour item. In the case of creating new item and left
		  merge item pos will be equal to dst_items.
		*/
		if (hint->control & SF_UPTIP && hint->result & SF_MOVIP) {
			hint->pos.item = (hint->control & SF_LEFT ?
					  dst_items : 0);
		}
	} else {
		if (plugin_call(src_item.plugin->o.item_ops,
				estimate_shift, &src_item,
				&dst_item, hint))
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
				dst_items : 0), ~0ul);
		
		if (node40_expand(dst_entity, &pos, hint->rest, 1)) {
			aal_exception_error("Can't expand node for "
					    "shifting units into it.");
			return -EINVAL;
		}

		hint->items++;
		
		/* Setting up new item fields */
		ih = node40_ih_at(dst_node, pos.item);
		ih40_set_pid(ih, src_item.plugin->h.id);

		aal_memcpy(&ih->key, src_item.key.body,
			   sizeof(ih->key));

		/*
		  Initializing dst item after it was created by node40_expand()
		  function.
		*/
		if (node40_item(dst_entity, &pos, &dst_item))
			return -EINVAL;

		if (dst_item.plugin->o.item_ops->init)
			dst_item.plugin->o.item_ops->init(&dst_item);

		/*
		  Setting item len to old len, that is zero, as it was just
		  created. This is needed for correct work of shift() method of
		  some items, which do not have "units" field and calculate the
		  number of units by own len, like extent40 does. This is
		  because, extent40 has all units of the same length.
		*/
		dst_item.len = 0;
	} else {
		/*
		  Items are mergeable, so we do not need to create new item in
		  the dst node. We just need to expand existent dst item by
		  hint->rest. So, we will call node40_expand() with unit
		  component not equal ~0ul.
		*/
		POS_INIT(&pos, (hint->control & SF_LEFT ?
				dst_items - 1 : 0), 0);

		if (node40_expand(dst_entity, &pos, hint->rest, 1)) {
			aal_exception_error("Can't expand item for "
					    "shifting units into it.");
			return -EINVAL;
		}
	}

	overhead = 0;

	if (src_item.plugin->o.item_ops->overhead) {
		overhead = plugin_call(src_item.plugin->o.item_ops,
				       overhead, &src_item);
	}

	/*
	  As @hint->rest is number of bytes units occupy, we decrease it by item
	  overhead.
	*/
	if (hint->create)
		hint->rest -= overhead;
	
	/* Shift units from @src_item to @dst_item */
	if (plugin_call(src_item.plugin->o.item_ops, shift,
			&src_item, &dst_item, hint))
	{
		return -EINVAL;
	}

	/* Updating source node fields */
	pos.item = src_item.pos.item;

	/*
	  We will remove src_item if it has became empty and insert point is not
	  points it.
	*/
	remove = (hint->rest == (src_item.len - overhead) &&
		  (hint->result & SF_MOVIP || pos.item != hint->pos.item));
	
	/* Updating item's keys */
	if (hint->control & SF_LEFT) {
		/*
		  We do not need update key of the src item which is going to be
		  removed.
		*/
		if (!remove) {
			ih = node40_ih_at(src_node, src_item.pos.item);

			aal_memcpy(&ih->key, src_item.key.body,
				   sizeof(ih->key));
		}
	} else {
		ih = node40_ih_at(dst_node, dst_item.pos.item);

		aal_memcpy(&ih->key, dst_item.key.body,
			   sizeof(ih->key));
	}
	
	if (remove) {
		/*
		  Like node40_expand() does, node40_shrink() will remove pointed
		  item if unit component is ~0ul and shrink the item pointed by
		  pos if unit component is not ~0ul.
		*/
		pos.unit = ~0ul;
		len = src_item.len;

		/*
		  As item will be removed, we should update item pos in hint
		  properly.
		*/
		if (hint->control & SF_UPTIP && pos.item < hint->pos.item)
			hint->pos.item--;
	} else {
		pos.unit = 0;
		len = hint->rest;
	}

	return node40_shrink(src_entity, &pos, len, 1);
}

/*
  Predicts how many whole item may be shifted from @src_entity to
  @dst_entity.
*/
static errno_t node40_predict(object_entity_t *src_entity,
			      object_entity_t *dst_entity, 
			      shift_hint_t *hint)
{
	uint32_t flags;
	uint32_t space;
	
	uint32_t src_items;
	uint32_t dst_items;

	node40_t *src_node;
	node40_t *dst_node;

	item40_header_t *cur;
	item40_header_t *end;
	
	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	dst_items = node40_items(dst_entity);
	
	if (!(src_items = node40_items(src_entity)))
		return 0;

	space = node40_space(dst_entity);

	end = node40_ih_at(src_node,
			   src_items - 1);
	
	cur = (hint->control & SF_LEFT ?
	       node40_ih_at(src_node, 0) : end);
	
	/*
	  Estimating will be finished if @src_items value is exhausted or insert
	  point is shifted out to neighbour node.
	*/
	flags = hint->control;
	
	while (!(hint->result & SF_MOVIP) && src_items > 0) {
		uint32_t len;

		if (!(flags & SF_MOVIP) && (flags & SF_RIGHT)) {
			if (hint->pos.item >= src_items)
				break;
		}
		
		/* Getting length of current item */
		len = (cur == end ? nh40_get_free_space_start(src_node) :
		       ih40_get_offset(cur - 1)) - ih40_get_offset(cur);
		
		/*
		  We go out if there is no enough free space to shift one more
		  whole item.
		*/
		if (space < len + node40_overhead(dst_entity))
			break;

		if (flags & SF_UPTIP) {
			
			/* Updating insert position */
			if (flags & SF_LEFT) {
				if (hint->pos.item == 0) {
					pos_t pos;
					uint32_t units;
					item_entity_t item;

					/*
					  If unit component if zero, we can
					  shift whole item pointed by pos.
					*/
					POS_INIT(&pos, 0, ~0ul);
					
					if (node40_item(src_entity, &pos, &item))
						return -EINVAL;

					if (!item.plugin->o.item_ops->units)
						return -EINVAL;
				
					units = item.plugin->o.item_ops->units(&item);

					/*
					  Breaking if insert point reach the end
					  of node.
					*/
					if (flags & SF_MOVIP &&
					    (hint->pos.unit == ~0ul ||
					     hint->pos.unit >= units - 1))
					{
						/*
						  If we are permitted to move
						  insetr point to the neigbour,
						  we doing it.
						*/
						hint->result |= SF_MOVIP;
						hint->pos.item = dst_items;
					} else
						break;
				
				} else
					hint->pos.item--;
			} else {
				/*
				  Checking if insert point reach the end of
				  node. Hint is updated here.
				*/
				if (hint->pos.item >= src_items - 1) {
				
					if (hint->pos.item == src_items - 1) {

						if (flags & SF_MOVIP &&
						    (hint->pos.unit == ~0ul ||
						     hint->pos.unit == 0))
						{
							hint->result |= SF_MOVIP;
							hint->pos.item = 0;
						} else {
							if (hint->pos.unit != ~0ul)
								break;
						}
					} else {
						/*
						  Insert point at the unexistent
						  item at the end of node. So we
						  just update hint and breaking
						  the loop.
						*/
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
		space -= (len + node40_overhead(dst_entity));
	}

	/*
	  After number of whole items was estimated, all free space will be
	  used for estimating how many units may be shifted.
	*/
	hint->rest = space;

	return 0;
}

/* Moves some amount of whole items from @src_entity to @dst_entity */
static errno_t node40_transfuse(object_entity_t *src_entity,
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

	if ((res = node40_predict(src_entity, dst_entity, hint)))
		return res;

	/* No items to be shifted */
	if (hint->items == 0 || hint->bytes == 0)
		return 0;
	
	dst_items = node40_items(dst_entity);
	src_items = node40_items(src_entity);

	if (hint->control & SF_LEFT) {
		POS_INIT(&src_pos, 0, ~0ul);
		POS_INIT(&dst_pos, dst_items, ~0ul);
	} else {
		POS_INIT(&dst_pos, 0, ~0ul);
		POS_INIT(&src_pos, src_items - hint->items, ~0ul);
	}
	
	/*
	  Expanding dst node in order to making room for new items and
	  update node header.
	*/
	if (node40_expand(dst_entity, &dst_pos, hint->bytes,
			  hint->items))
	{
		return -EINVAL;
	}
		
	/* Copying items from src node to dst one */
	if (node40_rep(dst_entity, &dst_pos, src_entity, &src_pos,
		       hint->items))
	{
		return -EINVAL;
	}

	/*
	  Shrinking source node after items are copied from it to dst
	  node.
	*/
	if (node40_shrink(src_entity, &src_pos, hint->bytes,
			  hint->items))
	{
		return -EINVAL;
	}
	
	return 0;
}

/* Performs shift of items and units from @entity to @neighb */
static errno_t node40_shift(object_entity_t *src_entity,
			    object_entity_t *dst_entity,
			    shift_hint_t *hint)
{
	errno_t res;
	node40_t *src_node;
	node40_t *dst_node;
	shift_hint_t merge;

	aal_assert("umka-2050", src_entity != NULL);
	aal_assert("umka-2051", dst_entity != NULL);

	aal_assert("umka-2048", node40_loaded(src_entity));
	aal_assert("umka-2049", node40_loaded(dst_entity));

	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	merge = *hint;
	
	/*
	  First of all we should try to merge boundary items if they are
	  mergeable. This work is performed by unit shift methods with the
	  special shift flags SF_MERGE. It will forbid creating the new item if
	  boundary items are not mergeable.
	*/
	merge.control |= SF_MERGE;
	merge.rest = node40_space(dst_entity);
	
	/*
	  Merges nodes without ability to create the new item in the
	  @dst_node. This is needed for avoiding the case when a node will
	  contain two neighbour items which are mergeable. That would be not
	  optimal space usage and might also led to some unstable behavior of
	  the code which assume that next mergeable item lies in the neighbour
	  node, not the next to it (directory read and lookup code).
	*/
	if ((res = node40_merge(src_entity, dst_entity, &merge))) {
		aal_exception_error("Can't merge two nodes durring "
				    "node shift operation.");
		return res;
	}

	hint->pos = merge.pos;
	hint->result = merge.result;

	if (hint->result & SF_MOVIP)
		goto out_update_hint;

	/* Moving some amount of whole items from @src_node to @dst_node */
	if ((res = node40_transfuse(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't transfuse two nodes "
				    "durring node shift operation.");
		return res;
	}

	/*
	  Checking if insert point was not moved into the corresponding
	  neighbour.
	*/
	if (hint->result & SF_MOVIP)
		goto out_update_hint;

	/*
	  Merges border items with ability to create new item in the dst node.
	  Here our objective is to shift into neighbour node as many units as
	  possible.
	*/
	if ((res = node40_merge(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't merge two nodes durring "
				    "node shift operation.");
		return res;
	}

	/*
	  The case when insert point is moved to the neighbour node, but nothing
	  was shifted because old insert point was at last item and last unit.
	  Thus, insert unit request should be converted into insert item one by
	  means of clearing unit component of the insert point in shift hint.
	*/
	if (hint->control & SF_UPTIP &&
	    hint->result & SF_MOVIP &&
	    hint->units == 0 && hint->create)
	{
		hint->pos.unit = ~0ul;
	}

 out_update_hint:
	hint->units += merge.units;
	hint->items += merge.items;
	return 0;
}

extern errno_t node40_check(object_entity_t *entity,
			    uint8_t mode);

extern errno_t node40_copy(object_entity_t *dst_entity,
			   pos_t *dst_pos, 
			   object_entity_t *src_entity,
			   pos_t *src_pos, 
			   copy_hint_t *hint);

#endif

static reiser4_node_ops_t node40_ops = {
	.init		 = node40_init,
	.load		 = node40_load,
	.close		 = node40_close,
	.unload		 = node40_unload,
	
	.confirm	 = node40_confirm,
	
	.lookup		 = node40_lookup,
	.items		 = node40_items,
	
	.get_key	 = node40_get_key,
	.get_item        = node40_get_item,
	.get_level	 = node40_get_level,
		
#ifndef ENABLE_STAND_ALONE
	.get_mstamp	 = node40_get_mstamp,
	.get_fstamp      = node40_get_fstamp,
		
	.move		 = node40_move,
	.form		 = node40_form,
	.sync            = node40_sync,
	.isdirty         = node40_isdirty,
	.mkdirty         = node40_mkdirty,
	.mkclean         = node40_mkclean,
	.insert		 = node40_insert,
	.remove		 = node40_remove,
	.cut             = node40_cut,
	.check		 = node40_check,
	.print		 = node40_print,
	.shift		 = node40_shift,
	.shrink		 = node40_shrink,
	.expand		 = node40_expand,
	.copy            = node40_copy,
	.rep             = node40_rep,

	.overhead	 = node40_overhead,
	.maxspace	 = node40_maxspace,
	.space		 = node40_space,
	
	.set_key	 = node40_set_key,
	.set_level       = node40_set_level,
	.set_mstamp	 = node40_set_mstamp,
	.set_fstamp      = node40_set_fstamp
#endif
};

static reiser4_plugin_t node40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = NODE_REISER40_ID,
		.group = 0,
		.type = NODE_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "node40",
		.desc = "Node plugin for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.node_ops = &node40_ops
	}
};

static reiser4_plugin_t *node40_start(reiser4_core_t *c) {
	core = c;
	return &node40_plugin;
}

plugin_register(node40, node40_start, NULL);

