/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40.c -- reiser4 node plugin functions. */

#include "node40.h"
#include "node40_repair.h"

static reiser4_core_t *core = NULL;

/* Return current node key policy (key size in fact). */
inline uint32_t node40_key_pol(node40_t *node) {
	return plug_call(node->kplug->o.key_ops, bodysize);
}

/* Return item header void pointer by pos. As node40 is able to work with
   different item types (short keys, large ones), we do not use item struct at
   all. But preffer to use raw pointers along with macros for working with
   them. */
void *node40_ih_at(node40_t *node, uint32_t pos) {
	void *ih = node->block->data + node->block->size;
	uint32_t size = ih_size(node40_key_pol(node));
	return (ih - (size * (pos + 1)));
}

/* Retutrn item body by pos */
void *node40_ib_at(node40_t *node, uint32_t pos) {
	void *ih = node40_ih_at(node, pos);

	return node->block->data +
		ih_get_offset(ih, node40_key_pol(node));
}

/* Returns node level field. */
static uint8_t node40_get_level(node_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL);
	return nh_get_level((node40_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
static uint32_t node40_get_state(node_entity_t *entity) {
	aal_assert("umka-2091", entity != NULL);
	return ((node40_t *)entity)->state;
}

static void node40_set_state(node_entity_t *entity,
			     uint32_t state)
{
	aal_assert("umka-2092", entity != NULL);
	((node40_t *)entity)->state = state;
}

/* Returns node mkfs stamp. */
static uint32_t node40_get_mstamp(node_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL);
	return nh_get_mkfs_id((node40_t *)entity);
}

/* Returns node flush stamp. */
static uint64_t node40_get_fstamp(node_entity_t *entity) {
	aal_assert("vpf-645", entity != NULL);
	return nh_get_flush_id((node40_t *)entity);
}

/* Makes clone of @src_entity node in @dst_node. */
static errno_t node40_clone(node_entity_t *src_entity,
			    node_entity_t *dst_entity)
{
	aal_assert("umka-2308", src_entity != NULL);
	aal_assert("umka-2309", dst_entity != NULL);

	aal_memcpy(((node40_t *)dst_entity)->block->data,
		   ((node40_t *)src_entity)->block->data,
		   ((node40_t *)src_entity)->block->size);

	return 0;
}

/* Makes fresh node. That is sets numebr of items to zero, and reinitializes all
   other fields of node header. */
static errno_t node40_fresh(node_entity_t *entity,
			    uint8_t level)
{
	node40_t *node;
	uint32_t header;

	aal_assert("umka-2374", entity != NULL);
	
	node = (node40_t *)entity;
	
	nh_set_num_items(node, 0);
	nh_set_level(node, level);
	nh_set_magic(node, NODE40_MAGIC);
	nh_set_pid(node, node40_plug.id.id);

	header = sizeof(node40_header_t);
	nh_set_free_space_start(node, header);
	nh_set_free_space(node, node->block->size - header);

	return 0;
}

/* Saves node to device */
static errno_t node40_sync(node_entity_t *entity) {
	errno_t res;
	node40_t *node;
	
	aal_assert("umka-1552", entity != NULL);

	node = (node40_t *)entity;
	
	if ((res = aal_block_write(node->block)))
		return res;

	node->state &= ~(1 << ENTITY_DIRTY);
	
	return 0;
}

/* Moves node to passed @blk. */
void node40_move(node_entity_t *entity, blk_t nr) {
	aal_block_t *block;
	
	aal_assert("umka-2377", entity != NULL);

	block = ((node40_t *)entity)->block;
	aal_block_move(block, block->device, nr);
}
#endif

/* Closes node by means of closing its block */
static errno_t node40_fini(node_entity_t *entity) {
	aal_assert("umka-825", entity != NULL);

	aal_block_free(((node40_t *)entity)->block);
	aal_free(entity);
	return 0;
}

/* Returns item number in passed node entity. Used for any loops through the all
   node items. */
static uint32_t node40_items(node_entity_t *entity) {
	aal_assert("vpf-018", entity != NULL);
	return nh_get_num_items((node40_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns node free space */
static uint16_t node40_space(node_entity_t *entity) {
	aal_assert("vpf-020", entity != NULL);
	return nh_get_free_space((node40_t *)entity);
}

/* Sets node make stamp */
static void node40_set_mstamp(node_entity_t *entity,
			      uint32_t stamp)
{
	aal_assert("vpf-644", entity != NULL);

	nh_set_mkfs_id((node40_t *)entity, stamp);
	((node40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

/* Returns node flush stamp */
static void node40_set_fstamp(node_entity_t *entity,
			      uint64_t stamp)
{
	aal_assert("vpf-643", entity != NULL);
	
	nh_set_flush_id((node40_t *)entity, stamp);
	((node40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

/* Set new node level to @level. */
static void node40_set_level(node_entity_t *entity,
			     uint8_t level)
{
	aal_assert("umka-1864", entity != NULL);
	
	nh_set_level((node40_t *)entity, level);
	((node40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}
#endif

/* Returns length of item at pos. */
static uint16_t node40_len(node_entity_t *entity, 
			   pos_t *pos)
{
	void *ih;
	uint32_t pol;
	node40_t *node;
    
	aal_assert("umka-942", pos != NULL);
	aal_assert("vpf-037", entity != NULL);

	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	ih = node40_ih_at(node, pos->item);

	/* Item length is calculated as next item offset minus current item
	   offset. If we're on the last item then we use free space start
	   instead.*/
	if (pos->item == (node40_items(entity) - 1)) {
		return nh_get_free_space_start(node) -
			ih_get_offset(ih, pol);
	} else {
		return ih_get_offset((ih - ih_size(pol)), pol) -
			ih_get_offset(ih, pol);
	}
}

/* Initializes node and on @block with key plugin @kplug. Returns initialized
   node instance. */
static node_entity_t *node40_init(aal_block_t *block,
				  reiser4_plug_t *kplug)
{
	node40_t *node;
	
	aal_assert("umka-2376", kplug != NULL);
	aal_assert("umka-2375", block != NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	node->kplug = kplug;
	node->block = block;
	node->plug = &node40_plug;
	return (node_entity_t *)node;
}

/* Returns key at passed @pos */
static errno_t node40_get_key(node_entity_t *entity,
			      pos_t *pos, key_entity_t *key) 
{
	void *body;
	node40_t *node;
    
	aal_assert("umka-821", key != NULL);
	aal_assert("umka-939", pos != NULL);
	
	aal_assert("umka-2344", pos->item <
		   node40_items(entity));

	aal_assert("umka-2333", entity != NULL);

	node = (node40_t *)entity;
	body = node40_ih_at(node, pos->item);
	
	key->plug = ((node40_t *)entity)->kplug;

	aal_memcpy(key->body, body,
		   key_size(node40_key_pol(node)));
	
	return 0;
}

/* Initializes @place at @pos. Fetches all item fields. */
errno_t node40_fetch(node_entity_t *entity,
		     pos_t *pos, place_t *place)
{
	rid_t pid;
	uint32_t pol;
	node40_t *node;
	
	aal_assert("umka-1813", pos != NULL);
	aal_assert("umka-1602", place != NULL);
	aal_assert("umka-1631", entity != NULL);
	
	aal_assert("umka-2351", pos->item <
		   node40_items(entity));

	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	
	/* Initializing item's plugin */
	pid = ih_get_pid(node40_ih_at(node, pos->item), pol);
	
	if (!(place->plug = core->factory_ops.ifind(ITEM_PLUG_TYPE,
						    pid)))
	{
		aal_exception_error("Can't find item plugin by its "
				    "id 0x%x.", pid);
		return -EINVAL;
	}

	/* Initializing other fields */
	place->pos = *pos;
	place->block = node->block;
	place->len = node40_len(entity, pos);
	place->body = node40_ib_at(node, pos->item);

	/* Getting item key */
	return node40_get_key(entity, pos, &place->key);
}

#ifndef ENABLE_STAND_ALONE
/* Retutns item overhead for this node format. Widely used in modification and
   estimation routines. */
static uint16_t node40_overhead(node_entity_t *entity) {
	return ih_size(node40_key_pol((node40_t *)entity));
}

/* Returns maximal size of item possible for passed node instance */
static uint16_t node40_maxspace(node_entity_t *entity) {
	node40_t *node;
	
	aal_assert("vpf-016", entity != NULL);
	
	/* Maximal space is node size minus node header and minus item
	   header. */
	node = (node40_t *)entity;
	
	return (node->block->size - sizeof(node40_header_t) -
		ih_size(node40_key_pol(node)));
}

/* Calculates size of a region denoted by @pos and @count. This is used by
   node40_copy(), node40_remove(), etc. */
uint32_t node40_size(node40_t *node, pos_t *pos, uint32_t count) {
	void *ih;
	uint32_t len;
	uint32_t pol;

	pol = node40_key_pol(node);
	ih = node40_ih_at(node, pos->item);
	
	if (pos->item + count < nh_get_num_items(node)) {
		uint32_t offset = (ih_size(pol) * count);
		len = ih_get_offset((ih - offset), pol);
	} else {
		len = nh_get_free_space_start(node);
	}

	return len - ih_get_offset(ih, pol);
}

/* Makes expand passed @node by @len in odrer to make room for insert new
   items/units. This function is used by insert and shift methods. */
errno_t node40_expand(node_entity_t *entity, pos_t *pos,
		      uint32_t len, uint32_t count)
{
	void *ih;
	int insert;

	uint32_t pol;
	uint32_t item;
	node40_t *node;
	uint32_t items;
	uint32_t offset;
	uint32_t headers;

	aal_assert("vpf-006", pos != NULL);
	aal_assert("umka-817", entity != NULL);

	if (len == 0) return 0;
	node = (node40_t *)entity;

	insert = (pos->unit == MAX_UINT32);

	pol = node40_key_pol(node);
	items = nh_get_num_items(node);
	headers = count * ih_size(pol);

	/* Getting real pos of the item to be updated */
	item = pos->item + !insert;
	ih = node40_ih_at(node, item);

	/* If item pos is inside the range [0..count - 1], we should perform the
	   data moving and offset upadting. */
	if (item < items) {
		void *src, *dst;
		uint32_t i, size;

		/* Moving items bodies */
		offset = ih_get_offset(ih, pol);
		src = node->block->data + offset;
		dst = src + len;

		size = nh_get_free_space_start(node) - offset;

		aal_memmove(dst, src, size);

		/* Updating item offsets */
		for (i = 0; i < items - item; i++) {
			ih_inc_offset(ih, len, pol);
			ih -= ih_size(pol);
		}

		/* If this is the insert new item mode, we should prepare the
		   room for new item header and set it up. */
		if (insert) {
			src = node40_ih_at(node, items - 1);

			dst = node40_ih_at(node, items - 1 +
					   count);

			size = ih_size(pol) * (items - item);
			
			aal_memmove(dst, src, size);
		}

		ih = node40_ih_at(node, item);
	} else {
		offset = nh_get_free_space_start(node);
	}

	/* Updating node's free space and free space start fields */
	nh_inc_free_space_start(node, len);
	nh_dec_free_space(node, len);

	if (insert) {
                /* Setting up the fields of new item */
		ih_set_offset(ih, offset, pol);

		/* Setting up node header */
		nh_inc_num_items(node, count);
		nh_dec_free_space(node, headers);
	}

	node->state |= (1 << ENTITY_DIRTY);
	return 0;
}

/* General node40 cutting function. It is used from shift, remove, etc. It
   removes an amount of items specified by @count and shrinks node. */
errno_t node40_shrink(node_entity_t *entity, pos_t *pos,
		      uint32_t len, uint32_t count)
{
	uint32_t pol;
	uint32_t size;
	void *cur, *end;
	void *src, *dst;

	node40_t *node;
	uint32_t headers;
	uint32_t i, items;

	aal_assert("umka-1800", count > 0);
	aal_assert("umka-1799", pos != NULL);
	aal_assert("umka-1798", entity != NULL);

	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	items = nh_get_num_items(node);

	if (pos->unit == MAX_UINT32) {
		headers = count * ih_size(pol);
		end = node40_ih_at(node, items - 1);

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
				ih_size(pol);
	
			aal_memmove(dst, src, size);

			/* Updating item offsets */
			cur = node40_ih_at(node, pos->item);
	
			for (i = pos->item; i < items - count; i++) {
				ih_dec_offset(cur, len, pol);
				cur -= ih_size(pol);
			}
		}

		/* Updating node header */
		nh_dec_num_items(node, count);
		nh_inc_free_space(node, (len + headers));
	} else {
		void *ih;
		uint32_t item_len;
		node_entity_t *entity;

		entity = (node_entity_t *)node;

		ih = node40_ih_at(node, pos->item);
		item_len = node40_len(entity, pos);
		
		/* Moving item bodies */
		src = node40_ib_at(node, pos->item) + item_len;
		dst = src - len;

		size = nh_get_free_space_start(node) -
			ih_get_offset(ih, pol) - item_len;
		
		aal_memmove(dst, src, size);
		
		/* Updating header offsets */
		cur = ih - ih_size(pol);
		end = node40_ih_at(node, items - 1);
		
		while (cur >= end) {
			ih_dec_offset(cur, len, pol);
			cur -= ih_size(pol);
		}

		/* Updating node header and item header */
		nh_inc_free_space(node, len);
	}

	nh_dec_free_space_start(node, len);
	node->state |= (1 << ENTITY_DIRTY);
	
	return 0;
}

/* Makes copy of @count items from @src_entity to @dst_entity */
errno_t node40_copy(node_entity_t *dst_entity, pos_t *dst_pos,
		    node_entity_t *src_entity, pos_t *src_pos,
		    uint32_t count)
{
	uint32_t pol;
	uint32_t size;
	uint32_t items;
	uint32_t fss, i;
	uint32_t offset;
	uint32_t headers;

	void *body;
	void *ih, *end;
	void *src, *dst;
	
	node40_t *dst_node;
	node40_t *src_node;

	dst_node = (node40_t *)dst_entity;
	src_node = (node40_t *)src_entity;

	pol = node40_key_pol(dst_node);
	headers = count * ih_size(pol);
	items = nh_get_num_items(dst_node);
	fss = nh_get_free_space_start(dst_node);
	
	if (!(size = node40_size(src_node, src_pos, count)))
		return -EINVAL;
	
	/* Copying item bodies from src node to dst one */
	src = node40_ib_at(src_node, src_pos->item);

	if (dst_pos->item < items - count) {
		body = node40_ib_at(dst_node, dst_pos->item);
	} else {
		body = dst_node->block->data + fss - size;
	}
		
	aal_memcpy(body, src, size);

	/* Copying item headers from src node to dst one */
	src = node40_ih_at(src_node, src_pos->item +
			   count - 1);

	dst = node40_ih_at(dst_node, dst_pos->item +
			   count - 1);
			
	aal_memcpy(dst, src, headers);

	/* Updating item headers in dst node */
	end = node40_ih_at(dst_node, items - 1);
	ih = dst + (ih_size(pol) * (count - 1));
	
	offset = (body - dst_node->block->data);
	
	for (i = 0; i < count; i++) {
		uint32_t old;

		old = ih_get_offset(ih, pol);
		ih_set_offset(ih, offset, pol);
		
		if (ih == end) {
			offset += fss - ih_get_offset(ih, pol);
		} else {
			offset += ih_get_offset((ih - ih_size(pol)),
						pol) - old;
		}

		ih -= ih_size(pol);
	}
	
	dst_node->state |= (1 << ENTITY_DIRTY);
	return 0;
}

/* Mode modifying fucntion. */
int64_t node40_modify(node_entity_t *entity, pos_t *pos,
		      trans_hint_t *hint, modyfy_func_t modify_func)
{
        void *ih;
        uint32_t pol;
        uint32_t len;
        int64_t write;
        place_t place;
        node40_t *node;
                                                                                              
        node = (node40_t *)entity;
        len = hint->len + hint->overhead;
                                                                                              
        /* Makes expand of the node new items will be inserted in */
        if (node40_expand(entity, pos, len, 1)) {
                aal_exception_error("Can't expand node for insert "
                                    "item/unit.");
                return -EINVAL;
        }
                                                                                              
        pol = node40_key_pol(node);
        ih = node40_ih_at(node, pos->item);
                                                                                              
        /* Updating item header if we want insert new item */
        if (pos->unit == MAX_UINT32) {
                ih_set_pid(ih, hint->plug->id.id, pol);
                                                                                              
                aal_memcpy(ih, hint->offset.body,
                           key_size(pol));
        }
                                                                                              
        /* Preparing place for calling item plugin with them */
        if (node40_fetch(entity, pos, &place)) {
                aal_exception_error("Can't fetch item data.");
                return -EINVAL;
        }
                                                                                              
	/* Inserting units into @place */
	if ((write = modify_func(&place, hint)) < 0) {
		aal_exception_error("Can't insert unit to "
				    "node %llu, item %u.",
				    node->block->nr,
				    place.pos.item);
		return write;
	}
                                                                                              
        /* Updating item's key if we insert new item or if we insert unit into
           leftmost postion. */
        if (pos->unit == 0)
                aal_memcpy(ih, place.key.body, key_size(pol));
                                                                                              
        return write;
}

static errno_t node40_insert(node_entity_t *entity,
			     pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2448", pos != NULL);
	aal_assert("umka-1814", hint != NULL);
	aal_assert("umka-818", entity != NULL);

	return node40_modify(entity, pos, hint, 
			     hint->plug->o.item_ops->object->insert_units);
}

static int64_t node40_write(node_entity_t *entity,
			    pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2449", pos != NULL);
	aal_assert("umka-2450", hint != NULL);
	aal_assert("umka-2451", entity != NULL);

	return node40_modify(entity, pos, hint, 
			     hint->plug->o.item_ops->object->write_units);
}

/* Truncates node at @pos. Needed for tail conversion. */
static int64_t node40_trunc(node_entity_t *entity, pos_t *pos,
			    trans_hint_t *hint)
{
	void *ih;
	uint32_t pol;
	uint32_t len;
	int64_t count;
	place_t place;
	node40_t *node;
	
	aal_assert("umka-2462", pos != NULL);
	aal_assert("umka-2463", entity != NULL);

	node = (node40_t *)entity;
	pol = node40_key_pol(node);

	/* Getting item at @pos */
	if (node40_fetch(entity, pos, &place))
		return -EINVAL;

	/* Truncating item. */
	if ((count = plug_call(place.plug->o.item_ops->object,
			       trunc_units, &place, hint)) < 0)
	{
		return count;
	}

	len = hint->overhead + hint->len;
	
	/* Shrinking node and and update key. */
	if (len > 0) {
		errno_t res;
		uint32_t number;
		
		number = count;
		place.pos.unit = 0;
		
		if (len >= place.len) {
			number = 1;
			place.pos.unit = MAX_UINT32;
		}

		if ((res = node40_shrink(entity, &place.pos,
					 len, number)))
		{
			return res;
		}

		if (len < place.len) {
			ih = node40_ih_at(node, place.pos.item);
			aal_memcpy(ih, place.key.body, key_size(pol));
		}
	} else {
		ih = node40_ih_at(node, place.pos.item);
		aal_memcpy(ih, place.key.body, key_size(pol));
	}

	return count;
}

/* This function removes item/unit from the node at specified @pos */
errno_t node40_remove(node_entity_t *entity, pos_t *pos,
		      trans_hint_t *hint) 
{
	errno_t res;
	uint32_t pol;
	uint32_t len;
	place_t place;
	node40_t *node;
	uint32_t units;
	
	aal_assert("umka-987", pos != NULL);
	aal_assert("umka-986", entity != NULL);

	node = (node40_t *)entity;
	pol = node40_key_pol(node);

	if (node40_fetch(entity, pos, &place))
		return -EINVAL;

	/* Checking if we have to remove whole item as it will has not units
	   after removing. */
	units = plug_call(place.plug->o.item_ops->balance,
			  units, &place);
	
	if (units == hint->count)
		place.pos.unit = MAX_UINT32;
	
	if (place.pos.unit == MAX_UINT32) {
		hint->overhead = 0;
		
		if (!(hint->len = node40_size(node, &place.pos,
					      hint->count)))
		{
			return -EINVAL;
		}
	} else {
		if (place.plug->o.item_ops->object->remove_units) {
			/* Removing units from the item pointed by @pos */
			if ((res = plug_call(place.plug->o.item_ops->object,
					     remove_units, &place, hint)))
			{
				return res;
			}

			/* Updating items key if leftmost unit was changed */
			if (place.pos.unit == 0) {
				void *ih = node40_ih_at(node, place.pos.item);
				aal_memcpy(ih, place.key.body, key_size(pol));
			}
		}
	}

	/* Shrinking node by @len. */
	len = hint->len + hint->overhead;
	
	return node40_shrink(entity, &place.pos,
			     len, hint->count);
}

/* Fuses two mergeable items if they lie in the same node side by side. This is
   needed for fsck if it discovered, that two items are mergeable and lie in the
   same node (due to some corruption or fail) it will fuse them. */
static errno_t node40_fuse(node_entity_t *entity,  pos_t *left_pos,
			   pos_t *right_pos)
{
	errno_t res;
	uint32_t pol;
	int32_t delta;
	node40_t *node;
	uint32_t items;
	
	void *left, *right;
	place_t left_place;
	place_t right_place;
	
	aal_assert("umka-2682", entity != NULL);
	aal_assert("umka-2683", left_pos != NULL);
	aal_assert("umka-2684", right_pos != NULL);

	node = (node40_t *)entity;
	pol = node40_key_pol(node);
	items = node40_items(entity);

	aal_assert("umka-2685", (left_pos->item < items &&
				 right_pos->item < items));
	
	/* Check is items lie side by side. */
	delta = left_pos->item - right_pos->item;
	
	if (aal_abs(delta) > 1) {
		aal_exception_error("Can't fuse items which "
				    "lie side by side.");
		return -EINVAL;
	}

	/* Now we have to perform the following actions:

	   (1) Remove one of item headers (right one).
	
	   (2) Fuse items bodies. This stage means, that we should call some
	   item method, which will take into account item overhead, etc.
	*/
	left = node40_ih_at(node, left_pos->item);
	right = node40_ih_at(node, right_pos->item);

	/* First stage. Check if right item is last one. If so, we will not move
	   item headers at all. */
	if (right_pos->item < items - 1) {

		/* Eliminating @right_pos item header. */
		aal_memmove(right, (right - ih_size(pol)),
			    ih_size(pol));
	}

	nh_dec_num_items(node, 1);
	nh_inc_free_space(node, ih_size(pol));

	/* Second stage. Fusing item bodies. */
	if ((res = node40_fetch(entity, left_pos, &left_place))) {
		aal_exception_error("Can't fetch left item "
				    "during items fuse.");
		return res;
	}
	
	if ((res = node40_fetch(entity, right_pos, &right_place))) {
		aal_exception_error("Can't fetch right item "
				    "during items fuse.");
		return res;
	}

	aal_assert("umka-2686", plug_equal(left_place.plug,
					   right_place.plug));

	/* Check if item needs some special actions to fuse (like eliminate
	   header). If so, fuse items. */
	if (left_place.plug->o.item_ops->balance->fuse) {
		int32_t space;

		/* Returned space is released space in node and it should be
		   counted in node header. */
		space = plug_call(left_place.plug->o.item_ops->balance,
				  fuse, &left_place, &right_place);

		/* Updating node header. */
		nh_inc_free_space(node, space);
		nh_dec_free_space_start(node, space);
	}

	/* Now making node dirty. */
	node->state |= (1 << ENTITY_DIRTY);
	return 0;
}

/* Updates key at @pos by specified @key */
static errno_t node40_set_key(node_entity_t *entity, 
			      pos_t *pos, key_entity_t *key) 
{
	void *ih;
	node40_t *node;
	uint32_t key_size;

	aal_assert("umka-819", key != NULL);
    	aal_assert("umka-944", pos != NULL);
	
	aal_assert("umka-811", pos->item <
		   node40_items(entity));

	aal_assert("umka-809", entity != NULL);

	node = (node40_t *)entity;

	ih = node40_ih_at(node, pos->item);
	node->state |= (1 << ENTITY_DIRTY);
	
	key_size = key_size(node40_key_pol(node));
	aal_memcpy(ih, key->body, key_size);
		
	return 0;
}

/* Prepare text node description and push it into specified @stream. */
static errno_t node40_print(node_entity_t *entity, aal_stream_t *stream,
			    uint32_t start, uint32_t count, uint16_t options) 
{
	void *ih;
	pos_t pos;
	uint8_t level;
	uint32_t last, pol;	
	
	place_t place;
	node40_t *node;

	aal_assert("vpf-023", entity != NULL);
	aal_assert("umka-457", stream != NULL);

	node = (node40_t *)entity;
	level = node40_get_level(entity);
	
	aal_assert("umka-1580", level > 0);

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
			return -EINVAL;
		
		ih = node40_ih_at(node, pos.item);
		aal_stream_format(stream, "#%u, OFF %u: ", pos.item, 
				  ih_get_offset(ih, pol));
		
		/* Printing item by means of calling item print method if it is
		   implemented. If it is not, then print common item information
		   like key, len, etc. */
		if (place.plug->o.item_ops->debug->print) {
			if (plug_call(place.plug->o.item_ops->debug,
				      print, &place, stream, options))
			{
				return -EINVAL;
			}
		} else {
			char *key = core->key_ops.print(&place.key, PO_DEFAULT);
			
			aal_stream_format(stream, "PLUGIN: %s LEN=%u, KEY=[%s]\n",
					  place.plug->label, place.len, key);
		}
	}
	
	aal_stream_format(stream, "============================"
			  "===================================="
			  "==============\n");

	return 0;
}
#endif

/* Helper callback for comparing two keys. This is used by node lookup */
static int callback_comp_key(void *node, uint32_t pos,
			     void *key2, void *data)
{
	void *key1;
	
	aal_assert("umka-566", node != NULL);
	aal_assert("umka-567", key2 != NULL);
	aal_assert("umka-656", data != NULL);

	key1 = node40_ih_at((node40_t *)node, pos);

	return plug_call(((reiser4_plug_t *)data)->o.key_ops,
			 compraw, key1, key2);
}

/* Makes search inside the specified node @entity for @key and stores the result
   into @pos. This function returns 1 if key is found and 0 otherwise. */
static lookup_t node40_lookup(node_entity_t *entity,
			      key_entity_t *key,
			      bias_t bias, pos_t *pos)
{
	aal_assert("umka-472", key != NULL);
	aal_assert("umka-478", pos != NULL);
	aal_assert("umka-470", entity != NULL);
	aal_assert("umka-714", key->plug != NULL);

	switch (aux_bin_search(entity, node40_items(entity),
			       key->body, callback_comp_key,
			       key->plug, &pos->item))
	{
	case 1:
		return PRESENT;
	case 0:
		return ABSENT;
	default:
		return -EIO;
	}
}

#ifndef ENABLE_STAND_ALONE
/* Checks if two item entities are mergeable */
static int node40_mergeable(place_t *src, place_t *dst) {
	/* Check if plugins are equal */
	if (!plug_equal(src->plug, dst->plug))
		return 0;

	/* Check if mergeable is implemented and calling it if it is. */
	return src->plug->o.item_ops->balance->mergeable &&
		src->plug->o.item_ops->balance->mergeable(src, dst);
}

static int node40_splitable(place_t *place) {
	uint32_t units;
	
	/* Check if item's shift_units() and prep_shift() method are
	   implemented. */
	if (!place->plug->o.item_ops->balance->shift_units ||
	    !place->plug->o.item_ops->balance->prep_shift)
	{
		return 0;
	}
	
	/* We can't shift units from items with one unit */
	if (!place->plug->o.item_ops->balance->units)
		return 0;

	units = plug_call(place->plug->o.item_ops->balance,
			  units, place);

	/* Those item can be splitted that contains more than 1 unit or insert
	   point lies behind the last unit. */
	if (units > 1 || place->pos.unit >= units)
		return 1;
	
	return 0;
}

/* Initializes place by border item data (leftmost or rightmost). */
static errno_t node40_border(node_entity_t *entity,
			     int left_border,
			     place_t *place)
{
	pos_t pos;
	uint32_t items;
	
	aal_assert("umka-2669", place != NULL);
	aal_assert("umka-2670", entity != NULL);
	
	if ((items = node40_items(entity)) == 0)
		return -EINVAL;

	POS_INIT(&pos, (left_border ? 0 : items - 1),
		 MAX_UINT32);
	
	return node40_fetch(entity, &pos, place);
}

/* Merges border items of the src and dst nodes. The behavior depends on the
   passed hint pointer. */
static errno_t node40_unite(node_entity_t *src_entity,
			    node_entity_t *dst_entity, 
			    shift_hint_t *hint,
			    int create)
{
	pos_t pos;

	errno_t res;
	uint32_t pol;
	uint32_t len;
	uint32_t units;

	place_t src_place;
	place_t dst_place;
	
	node40_t *src_node;
	node40_t *dst_node;

	uint32_t dst_items;
	uint32_t src_items;
	
	void *src_ih, *dst_ih;
	int left_shift, remove_src;
	
	aal_assert("umka-1624", hint != NULL);
	aal_assert("umka-1622", src_entity != NULL);
	aal_assert("umka-1623", dst_entity != NULL);
	
	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	pol = node40_key_pol(dst_node);

	src_items = node40_items(src_entity);
	dst_items = node40_items(dst_entity);
	hint->rest = node40_space(dst_entity);
	
	if (src_items == 0 || hint->rest == 0)
		return 0;
	
	left_shift = (hint->control & SF_LEFT_SHIFT);
	
	/* We can't split the leftmost and rightmost items if they are the same
	   insetr point points to. */
	if (hint->pos.unit == MAX_UINT32) {
		if ((left_shift && hint->pos.item == 0) ||
		    (!left_shift && hint->pos.item == src_items))
		{
			return 0;
		}
	}

	/* Getting src item. */
	if ((res = node40_border(src_entity, left_shift, &src_place)))
		return res;

	/* Items that do not implement prep_shift() and shift_units() methods
	   cannot be splitted. */
	if (!node40_splitable(&src_place))
		return 0;
	
	/* Checking if items are mergeable */
	if (dst_items > 0) {
		/* Getting dst item. */
		if ((res = node40_border(dst_entity, !left_shift, &dst_place)))
			return res;

		/* Check if items has the same flags. If so, they can be tried
		   to be merged. */
		src_ih = node40_ih_at(src_node, src_place.pos.item);
		dst_ih = node40_ih_at(dst_node, dst_place.pos.item);

		if (ih_get_flags(src_ih, pol) != ih_get_flags(dst_ih, pol))
			return 0;

		/* Check if we need to create new new item in @dst_entity in
		   order to move data to it. */
		if (left_shift) {
			hint->create = !node40_mergeable(&dst_place, &src_place);
		} else {
			hint->create = !node40_mergeable(&src_place, &dst_place);
		}
	} else {
		hint->create = 1;
	}

	/* Calling item's pre_shift() method in order to estimate how many units
	   may be shifted out. This method also updates unit component of insert
	   point position. After this function is finish @hint->rest will
	   contain real number of bytes to be shifted into neighbour item. */
	if (hint->create) {
		uint32_t overhead;
		
		/* If items are not mergeable and we are in merge mode, we will
		   not create new item in dst node. This mode is needed for
		   mergeing mergeable items when they lie in different nodes.
		   And it prevents creating two mergeable items in the same
		   node. */
		if (!create)
			return 0;

		/* Getting node overhead in order to substract it from
		   @hint->rest, that is from space allowed to be used. */
		overhead = node40_overhead(dst_entity);

		/* There is not of enough free space in @dst_entity even to
		   create an empty item in it. Getting out. */
		if (hint->rest < overhead)
			return 0;

		/* Substract node overhead, that is item header. */
		hint->rest -= overhead;
			
		if (plug_call(src_place.plug->o.item_ops->balance,
			      prep_shift, &src_place, NULL, hint))
		{
			return -EINVAL;
		}

		/* Updating item component of insert point if it was moved into
		   neighbour item. */
		if (hint->control & SF_UPDATE_POINT &&
		    hint->result & SF_MOVE_POINT)
		{
			hint->pos.item = 0;
			
			if (left_shift)
				hint->pos.item = dst_items;
		}

		/* Check if shift_units() may shift something at all. If no --
		   getting out of here. */
		if (hint->units == 0)
			return 0;

		/* Prepare pos new item will be created at. */
		POS_INIT(&pos, (left_shift ? dst_items : 0), MAX_UINT32);
	} else {
		/* The same for case when we will not create new item, but will
		   shift units into existent one in neighbour node. */
		if (plug_call(src_place.plug->o.item_ops->balance,
			      prep_shift, &src_place, &dst_place, hint))
		{
			return -EINVAL;
		}

		if (hint->control & SF_UPDATE_POINT &&
		    hint->result & SF_MOVE_POINT)
		{
			hint->pos.item = 0;

			if (left_shift)
				hint->pos.item = dst_items - 1;
		}

		/* Check if shift_units() may shift something at all. If no --
		   getting out of here. */
		if (hint->units == 0)
			return 0;

		/* Prepare pos, item will be expanded at. Items are mergeable,
		   so we do not need to create new item in @dst_entity. We just
		   need to expand existent dst item by @hint->rest, thus unit
		   component of @pos is set to 0.*/
		POS_INIT(&pos, (left_shift ? dst_items - 1 : 0), 0);
	}

	/* Expanding node by @hint->rest at @pos. */
	if (node40_expand(dst_entity, &pos, hint->rest, 1)) {
		aal_exception_error("Can't expand node for "
				    "shifting units into it.");
		return -EINVAL;
	}

	if (hint->create) {
		/* Increasing number of shifted items. This is needed, because
		   higher abstraction levels will use it to determine was
		   something shifted or not. */
		hint->items++;
		
		/* Setting up new item fields such as plugin id and key. */
		dst_ih = node40_ih_at(dst_node, pos.item);

		ih_set_pid(dst_ih, src_place.plug->id.id, pol);
		aal_memcpy(dst_ih, src_place.key.body, key_size(pol));

		/* Copying old item flags to new created one. This is needed,
		   because these flags may say, for instance, that item is
		   already checked by fsck and thus, new item which is created
		   by splitting old one should have the same flags. This is also
		   needed, because items with different flags will not be merged
		   and this will cause bad tree packing. */
		src_ih = node40_ih_at(src_node, src_place.pos.item);
		ih_set_flags(dst_ih, ih_get_flags(src_ih, pol), pol); 

		/* Initializing @dst_place after that new item was created by
		   expand() function at it. */
		if (node40_fetch(dst_entity, &pos, &dst_place))
			return -EINVAL;
	}

	/* Shift units from @src_place to @dst_place. */
	if (plug_call(src_place.plug->o.item_ops->balance,
		      shift_units, &src_place, &dst_place, hint))
	{
		aal_exception_error("Can't shift units.");
		return -EINVAL;
	}

	pos.item = src_place.pos.item;

	/* Getting units number after shift. This is needed to detect correctly,
	   that src item is empty after shift and may be removed. */
	units = plug_call(src_place.plug->o.item_ops->balance,
			  units, &src_place);
	
	/* We will remove src item if it has became empty and insert point is
	   not points it, that is next insert will not be dealing with it. */
	remove_src = ((hint->rest == src_place.len || units == 0) &&
		      (hint->result & SF_MOVE_POINT ||
		       pos.item != hint->pos.item));
	
	/* Updating item's keys. */
	if (left_shift) {
		/* We do not need to update key of the src item which is going
		   to be removed. */
		if (!remove_src) {
			src_ih = node40_ih_at(src_node, src_place.pos.item);
			aal_memcpy(src_ih, src_place.key.body, key_size(pol));
		}
	} else {
		dst_ih = node40_ih_at(dst_node, dst_place.pos.item);
		aal_memcpy(dst_ih, dst_place.key.body, key_size(pol));
	}
	
	if (remove_src) {
		/* Like expand() does, shrink() will remove pointed item if unit
		   component is MAX_UINT32 and shrink the item pointed by pos if
		   unit component is not MAX_UINT32. */
		len = src_place.len;
		pos.unit = MAX_UINT32;

		/* As item will be removed, we should update item pos in hint
		   properly. */
		if (hint->control & SF_UPDATE_POINT &&
		    pos.item < hint->pos.item)
		{
			hint->pos.item--;
		}
	} else {
		/* Sources item will not be removed, because it is not yet
		   empty, it will be just shrinked by @hint->rest. */
		pos.unit = 0;
		len = hint->rest;
	}

	/* Shrining node by @len. */
	return node40_shrink(src_entity, &pos, len, 1);
}

/* Predicts how many whole item may be shifted from @src_entity to
   @dst_entity. */
static errno_t node40_predict(node_entity_t *src_entity,
			      node_entity_t *dst_entity, 
			      shift_hint_t *hint)
{
	uint32_t pol;
	uint32_t flags;
	uint32_t space;
	void *cur, *end;
	
	uint32_t src_items;
	uint32_t dst_items;

	node40_t *src_node;
	node40_t *dst_node;
	
	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	dst_items = node40_items(dst_entity);
	
	if (!(src_items = node40_items(src_entity)))
		return 0;

	pol = node40_key_pol(src_node);
	space = node40_space(dst_entity);
	end = node40_ih_at(src_node, src_items - 1);

	if (hint->control & SF_LEFT_SHIFT) {
		cur = node40_ih_at(src_node, 0);
	} else {
		cur = node40_ih_at(src_node, src_items - 1);
	}
	
	/* Estimating will be finished if @src_items value is exhausted or
	   insert point is shifted out to neighbour node. */
	flags = hint->control;
	
	while (!(hint->result & SF_MOVE_POINT) && src_items > 0) {
		uint32_t len;

		if (!(flags & SF_MOVE_POINT) && (flags & SF_RIGHT_SHIFT)) {
			if (hint->pos.item >= src_items)
				break;
		}
		
		/* Getting length of current item. */
		if (cur == end) {
			len = nh_get_free_space_start(src_node) -
				ih_get_offset(cur, pol);
		} else {
			len = ih_get_offset((cur - ih_size(pol)), pol) -
				ih_get_offset(cur, pol);
		}

		/* We go out if there is no enough free space to shift one more
		   whole item. */
		if (space < len + node40_overhead(dst_entity))
			break;

		/* Check if we allowed to update insert point. If so, we will do
		   so and loop probably will be breaked due to insert point
		   moved to neihgbour node. Will continue until there is enough
		   of space otherwise. */
		if (flags & SF_UPDATE_POINT) {
			/* Updating insert point. */
			if (flags & SF_LEFT_SHIFT) {
				if (hint->pos.item == 0) {
					pos_t pos;
					place_t place;
					uint32_t units;

					/* If unit component if zero, we can
					   shift whole item pointed by pos. */
					POS_INIT(&pos, 0, MAX_UINT32);
					
					if (node40_fetch(src_entity, &pos, &place))
						return -EINVAL;

					if (!place.plug->o.item_ops->balance->units)
						return -EINVAL;
				
					units = plug_call(place.plug->o.item_ops->balance,
							  units, &place);

					/* Breaking if insert point reach the
					   end of node. */
					if (flags & SF_MOVE_POINT &&
					    (hint->pos.unit == MAX_UINT32 ||
					     hint->pos.unit >= units - 1))
					{
						/* If we are permitted to move
						   insetr point to the neigbour,
						   we doing it. */
						hint->result |= SF_MOVE_POINT;
						hint->pos.item = dst_items;
					} else
						break;
				
				} else {
					hint->pos.item--;
				}
			} else {
				/* Checking if insert point reach the end of
				   node. */
				if (hint->pos.item >= src_items - 1) {
				
					if (hint->pos.item == src_items - 1) {
						/* Updating insert point to be
						   lie in neighbour node. */
						if (flags & SF_MOVE_POINT &&
						    (hint->pos.unit == MAX_UINT32 ||
						     hint->pos.unit == 0))
						{
							hint->result |= SF_MOVE_POINT;
							hint->pos.item = 0;
						} else {
							if (hint->pos.unit != MAX_UINT32)
								break;
						}
					} else {
						/* Insert point stays at the non
						   existent item. So we should
						   just update hint and break
						   the loop. */
						if (flags & SF_MOVE_POINT) {
							hint->result |= SF_MOVE_POINT;
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
		
		space -= (len + node40_overhead(dst_entity));
		cur += (flags & SF_LEFT_SHIFT ? -ih_size(pol) : ih_size(pol));
	}

	/* After number of whole items was estimated, all free space will be
	   used for estimating how many units may be shifted. */
	hint->rest = space;
	return 0;
}

/* Moves some amount of whole items from @src_entity to @dst_entity */
static errno_t node40_transfuse(node_entity_t *src_entity,
				node_entity_t *dst_entity, 
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

	/* Calculating how many items and how many bytes may be moved from
	   @src_entity to @dst_entity. Calculating result is stored in @hint and
	   will be used later. */
	if ((res = node40_predict(src_entity, dst_entity, hint)))
		return res;

	/* No items to be shifted */
	if (hint->items == 0 || hint->bytes == 0)
		return 0;
	
	dst_items = node40_items(dst_entity);
	src_items = node40_items(src_entity);

	/* Initializing src and dst positions, we will used them for moving
	   items. They are initialized in different way for left and right
	   shift. */
	if (hint->control & SF_LEFT_SHIFT) {
		POS_INIT(&src_pos, 0, MAX_UINT32);
		POS_INIT(&dst_pos, dst_items, MAX_UINT32);
	} else {
		uint32_t pos = src_items - hint->items;
		
		POS_INIT(&dst_pos, 0, MAX_UINT32);
		POS_INIT(&src_pos, pos, MAX_UINT32);
	}
	
	/* Expanding dst node in order to make room for new items and update
	   node header. */
	if ((res = node40_expand(dst_entity, &dst_pos,
				 hint->bytes, hint->items)))
	{
		return res;
	}
		
	/* Copying items from src node to dst one */
	if ((res = node40_copy(dst_entity, &dst_pos, src_entity,
			       &src_pos, hint->items)))
	{
		return res;
	}

	/* Shrinking source node after items are copied from it into dst
	   node. */
	return node40_shrink(src_entity, &src_pos, hint->bytes,
			     hint->items);
}

/* Performs shift of items and units from @src_entity to @dst_entity.

   Shift is performed in three passes:

   (1) This pass is supposed to merge two border items in @src_entity and
   @dst_entity if they are mergeable at all. This is needed to prevent creation
   of mergeable items in the same node during balancing.

   (2) Second pass is supposed to move as many whole items from @src_entity to
   @dst_entity as possible. That is exactly how many how does @dst_entity free
   space allow. This is actually main job of shift() function.

   (3) And finally third pass should again merge two border items in @src_entity
   and @dst_entity after some number of whole items was moved to @dst_entity. It
   is needed to use the rest of space remaining after whole items shift in
   second pass. */
static errno_t node40_shift(node_entity_t *src_entity,
			    node_entity_t *dst_entity,
			    shift_hint_t *hint)
{
	errno_t res;
	node40_t *src_node;
	node40_t *dst_node;

	aal_assert("umka-2050", src_entity != NULL);
	aal_assert("umka-2051", dst_entity != NULL);

	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	/* First pass is merge border items. Heer we check is we are allowed to
	   merge items. */
	if (hint->control & SF_ALLOW_MERGE) {
		if ((res = node40_unite(src_entity, dst_entity, hint, 0))) {
			aal_exception_error("Can't merge two nodes during "
					    "node shift operation.");
			return res;
		}
	} else {
		int left_shift;
		place_t src_place;
		place_t dst_place;
		
		/* Merge is not allowed by @hint->control flags. Check if border
		   items are mergeable. If so, we can't move at least one whole
		   item to @dst_entity, because we have to support all tree
		   invariants and namely there should not be mergeable items in
		   the same node. */
		left_shift = (hint->control & SF_LEFT_SHIFT);

		/* Getting border items and checking if they are mergeable. */
		if ((res = node40_border(src_entity, left_shift, &src_place)))
			return res;

		if ((res = node40_border(dst_entity, !left_shift, &dst_place)))
			return res;
		
		if (node40_mergeable(&src_place, &dst_place))
			return 0;
	}

	/* Check if insert point is moved to @dst_entity. If so then shift is
	   finished. */
	if (hint->result & SF_MOVE_POINT)
		return 0;

	/* Second pass is started here. Moving some amount of whole items from
	   @src_entity to @dst_entity. */
	if ((res = node40_transfuse(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't transfuse two nodes "
				    "during node shift operation.");
		return res;
	}

	/* Checking if insert point was moved into @dst_entity. If so then shift
	   gets out. */
	if (hint->result & SF_MOVE_POINT) {
		hint->units = 0;
		return 0;
	}

	/* Third pass is started here. Merges border items with ability to
	   create new item in the @dst_entity. Here our objective is to shift
	   into neighbour node as many units as possible and thus, to fill it
	   up. */
	if (hint->control & SF_ALLOW_MERGE) {
		if ((res = node40_unite(src_entity, dst_entity, hint, 1))) {
			aal_exception_error("Can't merge two nodes during"
					    "node shift operation.");
			return res;
		}
	}

	/* Here is handling the case when insert point is moved to the
	   @dst_entity, but nothing was actually shifted because old insert
	   point was at rightmost node position (last item unexistent unit).
	   Thus, we should set insert point unit component to MAX_UINT32 and
	   thus, to let code on higher abstraction levels know, that unit insert
	   operation should be converted to item insert (create) one. */
	if (hint->control & SF_UPDATE_POINT &&
	    hint->result & SF_MOVE_POINT &&
	    hint->units == 0 && hint->create)
	{
		hint->pos.unit = MAX_UINT32;
	}

	return 0;
}
#endif

static reiser4_node_ops_t node40_ops = {
	.init		= node40_init,
	.fini		= node40_fini,
	.lookup		= node40_lookup,
	.fetch          = node40_fetch,
	.items		= node40_items,
	
	.get_key	= node40_get_key,
	.get_level	= node40_get_level,
		
#ifndef ENABLE_STAND_ALONE
	.move		= node40_move,
	.clone          = node40_clone,
	.fresh		= node40_fresh,
	.sync           = node40_sync,
	.fuse           = node40_fuse,

	.pack           = node40_pack,
	.unpack         = node40_unpack,
	
	.insert		= node40_insert,
	.write		= node40_write,
	.trunc          = node40_trunc,
	.remove		= node40_remove,
	.print		= node40_print,
	.shift		= node40_shift,
	.shrink		= node40_shrink,
	.expand		= node40_expand,
	.merge          = node40_merge,
	.copy           = node40_copy,

	.overhead	= node40_overhead,
	.maxspace	= node40_maxspace,
	.space		= node40_space,
	
	.set_key	= node40_set_key,
	.set_level      = node40_set_level,

	.get_mstamp	= node40_get_mstamp,
	.get_fstamp     = node40_get_fstamp,
	
	.set_mstamp	= node40_set_mstamp,
	.set_fstamp     = node40_set_fstamp,
	
	.set_flag	= node40_set_flag,
	.clear_flag	= node40_clear_flag,
	.test_flag	= node40_test_flag,

	.set_state      = node40_set_state,
	.get_state      = node40_get_state,
	.check_struct	= node40_check_struct
#endif
};

reiser4_plug_t node40_plug = {
	.cl    = class_init,
	.id    = {NODE_REISER40_ID, 0, NODE_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "node40",
	.desc  = "Node plugin for reiser4, ver. " VERSION,
#endif
	.o = {
		.node_ops = &node40_ops
	}
};

static reiser4_plug_t *node40_start(reiser4_core_t *c) {
	core = c;
	return &node40_plug;
}

plug_register(node40, node40_start, NULL);
