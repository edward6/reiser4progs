/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40.c -- reiser4 node plugin functions. */

#include "node40.h"
#include "node40_repair.h"

extern reiser4_plug_t node40_plug;
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

#ifndef ENABLE_STAND_ALONE
/* Functions for dirtying nodes. */
int node40_isdirty(node_entity_t *entity) {
	aal_assert("umka-2091", entity != NULL);
	return ((node40_t *)entity)->block->dirty;
}

void node40_mkdirty(node_entity_t *entity) {
	aal_assert("umka-2092", entity != NULL);
	((node40_t *)entity)->block->dirty = 1;
}

void node40_mkclean(node_entity_t *entity) {
	aal_assert("umka-2093", entity != NULL);
	((node40_t *)entity)->block->dirty = 0;
}
#endif

/* Returns node level field. */
static uint8_t node40_get_level(node_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL);
	return nh_get_level((node40_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
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
	
	aal_assert("umka-1552", entity != NULL);
	
	if ((res = aal_block_write(((node40_t *)entity)->block)))
		return res;

	node40_mkclean(entity);
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
	
	node40_mkdirty(entity);
	nh_set_mkfs_id((node40_t *)entity, stamp);
}

/* Returns node flush stamp */
static void node40_set_fstamp(node_entity_t *entity,
			      uint64_t stamp)
{
	aal_assert("vpf-643", entity != NULL);
	
	node40_mkdirty(entity);
	nh_set_flush_id((node40_t *)entity, stamp);
}

/* Set new node level to @level. */
static void node40_set_level(node_entity_t *entity,
			     uint8_t level)
{
	aal_assert("umka-1864", entity != NULL);
	
	node40_mkdirty(entity);
	nh_set_level((node40_t *)entity, level);
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

	node40_mkdirty(entity);
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
	node40_mkdirty(entity);
	
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
	
	node40_mkdirty(dst_entity);
	return 0;
}

/* Mode modifying fucntion. */
static int64_t node40_mod(node_entity_t *entity, pos_t *pos,
			  trans_hint_t *hint, bool_t insert)
{
	void *ih;
	uint32_t pol;
	uint32_t len;
	int64_t write;
	place_t place;
	node40_t *node;
    
	len = hint->len + hint->ohd;
    
	/* Makes expand of the node new items will be inserted in */
	if (node40_expand(entity, pos, len, 1)) {
		aal_exception_error("Can't expand node for insert "
				    "item/unit.");
		return -EINVAL;
	}

	node = (node40_t *)entity;

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

	if (pos->unit == MAX_UINT32) {
		/* Calling item plugin to perform initializing the item */
		if (hint->plug->o.item_ops->init)
			hint->plug->o.item_ops->init(&place);
	}

	if (insert) {
		/* Inserting units into @place */
		if (!(write = plug_call(hint->plug->o.item_ops,
					insert, &place, hint)) < 0)
		{
			aal_exception_error("Can't insert unit to "
					    "node %llu.", node->block->nr);
			return write;
		}
	} else {
		/* Writes data into @place */
		if (!(write = plug_call(hint->plug->o.item_ops,
					write, &place, hint)) < 0)
		{
			aal_exception_error("Can't write data to "
					    "node %llu.", node->block->nr);
			return write;
		}
	}
	
	/* Updating item's key if we insert new item or if we insert unit into
	   leftmost postion. */
	if (pos->unit == 0) {
		aal_memcpy(ih, place.key.body,
			   key_size(pol));
	}

	return write;
}

static errno_t node40_insert(node_entity_t *entity,
			     pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2448", pos != NULL);
	aal_assert("umka-1814", hint != NULL);
	aal_assert("umka-818", entity != NULL);

	return node40_mod(entity, pos, hint, 1);
}

static int64_t node40_write(node_entity_t *entity,
			    pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2449", pos != NULL);
	aal_assert("umka-2450", hint != NULL);
	aal_assert("umka-2451", entity != NULL);

	return node40_mod(entity, pos, hint, 0);
}

/* Truncates node at @pos. Needed for tail conversion. */
static int64_t node40_truncate(node_entity_t *entity, pos_t *pos,
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

	/* Truncating item by its method truncate(). */
	if ((count = plug_call(place.plug->o.item_ops,
			       truncate, &place, hint)) < 0)
	{
		return count;
	}

	len = hint->ohd + hint->len;
	
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
	units = plug_call(place.plug->o.item_ops,
			  units, &place);
	
	if (units == hint->count) {
		place.pos.unit = MAX_UINT32;
	}
	
	if (place.pos.unit == MAX_UINT32) {
		hint->ohd = 0;
		
		if (!(hint->len = node40_size(node, &place.pos,
					      hint->count)))
		{
			return -EINVAL;
		}
	} else {
		if (place.plug->o.item_ops->remove) {
			/* Removing units from the item pointed by @pos */
			if ((res = plug_call(place.plug->o.item_ops,
					     remove, &place, hint)))
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
	len = hint->len + hint->ohd;
	return node40_shrink(entity, &place.pos, len, hint->count);
}

/* Updates key at @pos by specified @key */
static errno_t node40_set_key(node_entity_t *entity, 
			      pos_t *pos, key_entity_t *key) 
{
	void *ih;
	node40_t *node;

	aal_assert("umka-819", key != NULL);
    	aal_assert("umka-944", pos != NULL);
	
	aal_assert("umka-811", pos->item <
		   node40_items(entity));

	aal_assert("umka-809", entity != NULL);

	node = (node40_t *)entity;

	ih = node40_ih_at(node, pos->item);

	aal_memcpy(ih, key->body,
		   key_size(node40_key_pol(node)));

	node40_mkdirty(entity);
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
		aal_stream_format(stream, "#%u, LOC %u: ", pos.item, 
				  ih_get_offset(ih, pol));
		
		/* Printing item by means of calling item print method */
		if (place.plug->o.item_ops->print) {
			if (place.plug->o.item_ops->print(&place, stream,
							  options))
			{
				return -EINVAL;
			}
		} else {
			aal_stream_format(stream, "Method \"print\" is "
					  "not implemented for \"%s\".",
					  place.plug->label);
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
			     body_t *key2, void *data)
{
	body_t *key1;
	
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
static bool_t node40_mergeable(place_t *src, place_t *dst) {
	/* Check if plugins are equal */
	if (!plug_equal(src->plug, dst->plug))
		return FALSE;

	/* Check if mergeable is implemented and calling it if it is. */
	return src->plug->o.item_ops->mergeable &&
		src->plug->o.item_ops->mergeable(src, dst);
}

static bool_t node40_splitable(place_t *place) {
	uint32_t units;
	
	/* Check if item has shift() and estimate_shift() method are
	   implemented. */
	if (!place->plug->o.item_ops->shift)
		return FALSE;
	
	if (!place->plug->o.item_ops->estimate_shift)
		return FALSE;
	
	/* We can't shift units from items with one unit */
	if (!place->plug->o.item_ops->units)
		return FALSE;

	units = place->plug->o.item_ops->units(place);

	/* Those item can be splitted that contains more than 1 unit or insert
	   point lies behind the last unit. */
	if (units > 1 || place->pos.unit >= units)
		return TRUE;
	
	return FALSE;
}

/* Merges border items of the src and dst nodes. The behavior depends on the
   passed hint pointer. */
static errno_t node40_unite(node_entity_t *src_entity,
			    node_entity_t *dst_entity, 
			    shift_hint_t *hint)
{
	pos_t pos;
	int remove;
	
	uint32_t pol;
	uint32_t len;

	uint32_t overhead;
	place_t src_place;
	place_t dst_place;
	
	node40_t *src_node;
	node40_t *dst_node;

	uint32_t dst_items;
	uint32_t src_items;
	
	void *src_ih, *dst_ih;
	
	aal_assert("umka-1624", hint != NULL);
	aal_assert("umka-1622", src_entity != NULL);
	aal_assert("umka-1623", dst_entity != NULL);
	
	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	pol = node40_key_pol(dst_node);
	src_items = node40_items(src_entity);
	dst_items = node40_items(dst_entity);

	if (src_items == 0 || hint->rest == 0)
		return 0;
	
	/* We can't split the first and last items if they lie in position
	   insert point points to. */
	if (hint->control & MSF_LEFT) {
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
	POS_INIT(&pos, (hint->control & MSF_LEFT ? 0 :
			src_items - 1), MAX_UINT32);
	
	if (node40_fetch(src_entity, &pos, &src_place))
		return -EINVAL;

	/* Items that do not implement estimate_shift() and shift() methods
	   cannot be splitted. */
	if (!node40_splitable(&src_place))
		return 0;
	
	/* Checking if items are mergeable */
	if (dst_items > 0) {
		POS_INIT(&pos, (hint->control & MSF_LEFT ?
				dst_items - 1 : 0), MAX_UINT32);
		
		if (node40_fetch(dst_entity, &pos, &dst_place))
			return -EINVAL;

		/* Check if items has the same flags. If so, they can be
		   merged. */
		src_ih = node40_ih_at(src_node, src_place.pos.item);
		dst_ih = node40_ih_at(dst_node, dst_place.pos.item);

		if (ih_get_flags(src_ih, pol) != ih_get_flags(dst_ih, pol))
			return 0;
		
		if (hint->control & MSF_LEFT) {
			hint->create = !node40_mergeable(&dst_place,
							 &src_place);
		} else {
			hint->create = !node40_mergeable(&src_place,
							 &dst_place);
		}
	} else {
		hint->create = 1;
	}

	/* Calling item's predict() method in order to estimate how many units
	   may be shifted out. This method also updates unit component of insert
	   point position. After this function is finish @hint->rest will
	   contain real number of bytes to be shifted into neighbour item. */
	if (hint->create) {
		uint32_t overhead;

		/* If items are not mergeable and we are in merge mode, we
		   will not create new item in dst node. This mode is needed for
		   mergeing mergeable items when they lie in the same node. */
		if (hint->control & MSF_MERGE)
			return 0;
		
		overhead = node40_overhead(dst_entity);
		
		/* In the case items are not mergeable, we need count also item
		   overhead, because new item will be created. */
		if (hint->rest < overhead)
			return 0;

		hint->rest -= overhead;

		if (plug_call(src_place.plug->o.item_ops, estimate_shift,
			      &src_place, NULL, hint))
		{
			return -EINVAL;
		}

		/* Updating item component of the insert point if it was moved
		   into neighbour item. In the case of creating new item and
		   left merge item pos will be equal to dst_items. */
		if (hint->control & MSF_IPUPDT && hint->result & MSF_IPMOVE) {
			hint->pos.item = (hint->control & MSF_LEFT ?
					  dst_items : 0);
		}
	} else {
		if (plug_call(src_place.plug->o.item_ops, estimate_shift,
			      &src_place, &dst_place, hint))
		{
			return -EINVAL;
		}

		if (hint->control & MSF_IPUPDT && hint->result & MSF_IPMOVE) {
			hint->pos.item = (hint->control & MSF_LEFT ?
					  dst_items - 1 : 0);
		}
	}

	/* Units shift code starting here */
	if (hint->units == 0)
		return 0;
	
	if (hint->create) {
		/* Expanding dst node with creating new item */
		POS_INIT(&pos, (hint->control & MSF_LEFT ?
				dst_items : 0), MAX_UINT32);
		
		if (node40_expand(dst_entity, &pos, hint->rest, 1)) {
			aal_exception_error("Can't expand node for "
					    "shifting units into it.");
			return -EINVAL;
		}

		hint->items++;
		
		/* Setting up new item fields */
		dst_ih = node40_ih_at(dst_node, pos.item);
		ih_set_pid(dst_ih, src_place.plug->id.id, pol);

		aal_memcpy(dst_ih, src_place.key.body,
			   key_size(pol));

		/* Copying flags to new created item */
		src_ih = node40_ih_at(src_node, src_place.pos.item);
		ih_set_flags(dst_ih, ih_get_flags(src_ih, pol), pol); 

		/* Initializing dst item after it was created by node40_expand()
		   function. */
		if (node40_fetch(dst_entity, &pos, &dst_place))
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
		   hint->rest. So, we will call node40_expand() with unit
		   component not equal MAX_UINT32. */
		POS_INIT(&pos, (hint->control & MSF_LEFT ?
				dst_items - 1 : 0), 0);

		if (node40_expand(dst_entity, &pos, hint->rest, 1)) {
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
		  (hint->result & MSF_IPMOVE || pos.item != hint->pos.item));
	
	/* Updating item's keys */
	if (hint->control & MSF_LEFT) {
		/* We do not need update key of the src item which is going to
		   be removed. */
		if (!remove) {
			src_ih = node40_ih_at(src_node, src_place.pos.item);
			aal_memcpy(src_ih, src_place.key.body, key_size(pol));
		}
	} else {
		dst_ih = node40_ih_at(dst_node, dst_place.pos.item);
		aal_memcpy(dst_ih, dst_place.key.body, key_size(pol));
	}
	
	if (remove) {
		/* Like node40_expand() does, node40_shrink() will remove
		   pointed item if unit component is MAX_UINT32 and shrink the item
		   pointed by pos if unit component is not MAX_UINT32. */
		pos.unit = MAX_UINT32;
		len = src_place.len;

		/* As item will be removed, we should update item pos in hint
		   properly. */
		if (hint->control & MSF_IPUPDT && pos.item < hint->pos.item)
			hint->pos.item--;
	} else {
		pos.unit = 0;
		len = hint->rest;
	}

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

	if (hint->control & MSF_LEFT) {
		cur = node40_ih_at(src_node, 0);
	} else {
		cur = node40_ih_at(src_node, src_items - 1);
	}
	
	/* Estimating will be finished if @src_items value is exhausted or
	   insert point is shifted out to neighbour node. */
	flags = hint->control;
	
	while (!(hint->result & MSF_IPMOVE) && src_items > 0) {
		uint32_t len;

		if (!(flags & MSF_IPMOVE) && (flags & MSF_RIGHT)) {
			if (hint->pos.item >= src_items)
				break;
		}
		
		/* Getting length of current item */
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

		if (flags & MSF_IPUPDT) {
			
			/* Updating insert position */
			if (flags & MSF_LEFT) {
				if (hint->pos.item == 0) {
					pos_t pos;
					place_t place;
					uint32_t units;

					/* If unit component if zero, we can
					   shift whole item pointed by pos. */
					POS_INIT(&pos, 0, MAX_UINT32);
					
					if (node40_fetch(src_entity, &pos, &place))
						return -EINVAL;

					if (!place.plug->o.item_ops->units)
						return -EINVAL;
				
					units = place.plug->o.item_ops->units(&place);

					/* Breaking if insert point reach the
					   end of node. */
					if (flags & MSF_IPMOVE &&
					    (hint->pos.unit == MAX_UINT32 ||
					     hint->pos.unit >= units - 1))
					{
						/* If we are permitted to move
						   insetr point to the neigbour,
						   we doing it. */
						hint->result |= MSF_IPMOVE;
						hint->pos.item = dst_items;
					} else
						break;
				
				} else {
					hint->pos.item--;
				}
			} else {
				/* Checking if insert point reach the end of
				   node. Hint is updated here. */
				if (hint->pos.item >= src_items - 1) {
				
					if (hint->pos.item == src_items - 1) {

						if (flags & MSF_IPMOVE &&
						    (hint->pos.unit == MAX_UINT32 ||
						     hint->pos.unit == 0))
						{
							hint->result |= MSF_IPMOVE;
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
						if (flags & MSF_IPMOVE) {
							hint->result |= MSF_IPMOVE;
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
		cur += (flags & MSF_LEFT ? -ih_size(pol) : ih_size(pol));
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

	if ((res = node40_predict(src_entity, dst_entity, hint)))
		return res;

	/* No items to be shifted */
	if (hint->items == 0 || hint->bytes == 0)
		return 0;
	
	dst_items = node40_items(dst_entity);
	src_items = node40_items(src_entity);

	if (hint->control & MSF_LEFT) {
		POS_INIT(&src_pos, 0, MAX_UINT32);
		POS_INIT(&dst_pos, dst_items, MAX_UINT32);
	} else {
		POS_INIT(&dst_pos, 0, MAX_UINT32);

		POS_INIT(&src_pos, src_items - hint->items,
			 MAX_UINT32);
	}
	
	/* Expanding dst node in order to making room for new items and update
	   node header. */
	if (node40_expand(dst_entity, &dst_pos, hint->bytes,
			      hint->items))
	{
		return -EINVAL;
	}
		
	/* Copying items from src node to dst one */
	if (node40_copy(dst_entity, &dst_pos, src_entity,
			&src_pos, hint->items))
	{
		return -EINVAL;
	}

	/* Shrinking source node after items are copied from it to dst node. */
	if (node40_shrink(src_entity, &src_pos, hint->bytes,
			      hint->items))
	{
		return -EINVAL;
	}
	
	return 0;
}

/* Performs shift of items and units from @entity to @neighb */
static errno_t node40_shift(node_entity_t *src_entity,
			    node_entity_t *dst_entity,
			    shift_hint_t *hint)
{
	errno_t res;
	node40_t *src_node;
	node40_t *dst_node;
	shift_hint_t merge;

	aal_assert("umka-2050", src_entity != NULL);
	aal_assert("umka-2051", dst_entity != NULL);

	src_node = (node40_t *)src_entity;
	dst_node = (node40_t *)dst_entity;

	merge = *hint;
	
	/* First of all we should try to merge boundary items if they are
	   mergeable. This work is performed by unit shift methods with the
	   special shift flags MSF_MERGE. It will forbid creating the new item
	   if boundary items are not mergeable. */
	merge.control |= MSF_MERGE;
	merge.rest = node40_space(dst_entity);
	
	/* Merges passed nodes with no creating new item in the @dst_node. This
	   is needed for avoiding the case when a node will contain two
	   neighbour items which are mergeable. That would be not optimal space
	   usage and might also led to some unstable behavior of the code which
	   assume that next mergeable item lies in the neighbour node, not the
	   next to it (directory read and lookup code). */
	if ((res = node40_unite(src_entity, dst_entity, &merge))) {
		aal_exception_error("Can't merge two nodes durring "
				    "node shift operation.");
		return res;
	}

	hint->pos = merge.pos;
	hint->result = merge.result;

	if (hint->result & MSF_IPMOVE)
		goto out_update_hint;

	/* Moving some amount of whole items from @src_node to @dst_node */
	if ((res = node40_transfuse(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't transfuse two nodes "
				    "durring node shift operation.");
		return res;
	}

	/* Checking if insert point was not moved into the corresponding
	   neighbour. */
	if (hint->result & MSF_IPMOVE)
		goto out_update_hint;

	/* Merges border items with ability to create new item in the dst node.
	   Here our objective is to shift into neighbour node as many units as
	   possible. */
	if ((res = node40_unite(src_entity, dst_entity, hint))) {
		aal_exception_error("Can't merge two nodes durring "
				    "node shift operation.");
		return res;
	}

	/* The case when insert point is moved to the neighbour node, but
	   nothing was shifted because old insert point was at last item and
	   last unit.  Thus, insert unit request should be converted into insert
	   item one by means of clearing unit component of the insert point in
	   shift hint. */
	if (hint->control & MSF_IPUPDT &&
	    hint->result & MSF_IPMOVE &&
	    hint->units == 0 && hint->create)
	{
		hint->pos.unit = MAX_UINT32;
	}

 out_update_hint:
	hint->units += merge.units;
	hint->items += merge.items;
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
	.pack           = node40_pack,
	.unpack         = node40_unpack,
	.move		= node40_move,
	.clone          = node40_clone,
	.fresh		= node40_fresh,
	.sync           = node40_sync,
	
	.isdirty        = node40_isdirty,
	.mkdirty        = node40_mkdirty,
	.mkclean        = node40_mkclean,
	
	.insert		= node40_insert,
	.write		= node40_write,
	.truncate       = node40_truncate,
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
	.check_struct	= node40_check_struct
#endif
};

static reiser4_plug_t node40_plug = {
	.cl    = CLASS_INIT,
	.id    = {NODE40_ID, 0, NODE_PLUG_TYPE},
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
