/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40.c -- reiser4 node plugin functions. */

#include "node40.h"
#include "node40_repair.h"

reiser4_core_t *node40_core = NULL;

/* Return item header void pointer by pos. As node40 is able to work with
   different item types (short keys, large ones), we do not use item struct at
   all. But prefer to use raw pointers along with macros for working with
   them. */
void *node40_ih_at(reiser4_node_t *entity, uint32_t pos) {
	void *ih = entity->block->data + entity->block->size;
	return (ih - (ih_size(entity->keypol) * (pos + 1)));
}

#define node40_ib_by_ih(entity, ih) \
	entity->block->data + ih_get_offset(ih, entity->keypol)

/* Retutrn item body by pos */
void *node40_ib_at(reiser4_node_t *entity, uint32_t pos) {
	void *ih = node40_ih_at(entity, pos);

	return node40_ib_by_ih(entity, ih);
}

/* Returns node level field. */
uint8_t node40_get_level(reiser4_node_t *entity) {
	aal_assert("umka-1116", entity != NULL);
	return nh_get_level((reiser4_node_t *)entity);
}

reiser4_node_t *node40_prepare(aal_block_t *block, reiser4_key_plug_t *kplug) {
	reiser4_node_t *entity;
	
	aal_assert("umka-2376", kplug != NULL);
	aal_assert("umka-2375", block != NULL);
	
	if (!(entity = aal_calloc(sizeof(*entity), 0)))
		return NULL;

	entity->kplug = kplug;
	entity->block = block;
	entity->plug = &node40_plug;
	entity->keypol = plugcall(kplug, bodysize);

	return entity;
}

#ifndef ENABLE_MINIMAL
/* Functions for making node dirty, cleann and for check if it is dirty. This is
   used in all node modifying functions, etc. */
void node40_mkdirty(reiser4_node_t *entity) {
	aal_assert("umka-3016", entity != NULL);
	entity->block->dirty = 1;
}

void node40_mkclean(reiser4_node_t *entity) {
	aal_assert("umka-3017", entity != NULL);
	entity->block->dirty = 0;
}

int node40_isdirty(reiser4_node_t *entity) {
	aal_assert("umka-3018", entity != NULL);
	return entity->block->dirty;
}

uint32_t node40_get_state(reiser4_node_t *entity) {
	aal_assert("umka-2091", entity != NULL);
	return entity->state;
}

void node40_set_state(reiser4_node_t *entity, uint32_t state)
{
	aal_assert("umka-2092", entity != NULL);
	entity->state = state;
}

/* Returns node mkfs stamp. */
uint32_t node40_get_mstamp(reiser4_node_t *entity) {
	aal_assert("umka-1127", entity != NULL);
	return nh_get_mkfs_id(entity);
}

/* Returns node flush stamp. */
uint64_t node40_get_fstamp(reiser4_node_t *entity) {
	aal_assert("vpf-645", entity != NULL);
	return nh_get_flush_id(entity);
}

reiser4_node_t *
node40_init_common(aal_block_t *block, uint8_t level,
		   reiser4_key_plug_t *kplug,
		   reiser4_node_plug_t *nplug,
		   const uint32_t magic,
		   uint32_t node_header_size,
		   reiser4_node_t * (*prepare_fn)(aal_block_t *,
						  reiser4_key_plug_t *))
{
	reiser4_node_t *entity;

	aal_assert("umka-2374", block != NULL);
	aal_assert("vpf-1417",  kplug != NULL);

	if (!(entity = prepare_fn(block, kplug)))
		return NULL;

	nh_set_num_items(entity, 0);
	nh_set_level(entity, level);
	nh_set_magic(entity, magic);
	nh_set_pid(entity, nplug->p.id.id);
	nh_set_free_space_start(entity, node_header_size);
	nh_set_free_space(entity, block->size - node_header_size);

	return entity;
}

/*
 * Initializes node of the given @level on the @block with key plugin
 * @kplug. Returns initialized node instance
 */
static reiser4_node_t *node40_init(aal_block_t *block, uint8_t level,
				   reiser4_key_plug_t *kplug)
{
	return node40_init_common(block, level, kplug,
				  &node40_plug,
				  NODE40_MAGIC,
				  sizeof(node40_header_t),
				  node40_prepare);
}

/* Saves node to device */
errno_t node40_sync(reiser4_node_t *entity) {
	errno_t res;
	
	aal_assert("umka-1552", entity != NULL);

	if ((res = aal_block_write(entity->block)))
		return res;

	node40_mkclean(entity);
	return 0;
}
#endif

/* Closes node by means of closing its block */
errno_t node40_fini(reiser4_node_t *entity) {
	aal_assert("umka-825", entity != NULL);

	aal_block_free(entity->block);
	entity->plug = NULL;
	aal_free(entity);
	
	return 0;
}

/* Returns item number in passed node entity. Used for any loops through the all
   node items. */
uint32_t node40_items(reiser4_node_t *entity) {
	aal_assert("vpf-018", entity != NULL);
	return nh_get_num_items(entity);
}

#ifndef ENABLE_MINIMAL
/* Returns node free space. */
uint16_t node40_space(reiser4_node_t *entity) {
	aal_assert("vpf-020", entity != NULL);
	return nh_get_free_space(entity);
}

/* Sets node make stamp. */
void node40_set_mstamp(reiser4_node_t *entity, uint32_t stamp)
{
	aal_assert("vpf-644", entity != NULL);

	nh_set_mkfs_id(entity, stamp);
	node40_mkdirty(entity);
}

/* Returns node flush stamp */
void node40_set_fstamp(reiser4_node_t *entity, uint64_t stamp)
{
	aal_assert("vpf-643", entity != NULL);
	
	nh_set_flush_id(entity, stamp);
	node40_mkdirty(entity);
}

/* Set new node level to @level. */
void node40_set_level(reiser4_node_t *entity, uint8_t level)
{
	aal_assert("umka-1864", entity != NULL);
	
	nh_set_level(entity, level);
	node40_mkdirty(entity);
}
#endif

static uint16_t node40_len_by_ih(reiser4_node_t *entity, void *ih, 
				 pos_t *pos, uint16_t items) 
{
	uint8_t pol = entity->keypol;

	/* Item length is calculated as next item offset minus current item
	   offset. If we're on the last item then we use free space start
	   instead.*/
	if (pos->item + 1 == items) {
		return nh_get_free_space_start(entity) -
			ih_get_offset(ih, pol);
	} else {
		return ih_get_offset((ih - ih_size(pol)), pol) -
			ih_get_offset(ih, pol);
	}
}

/* Returns length of item at pos. */
uint16_t node40_len(reiser4_node_t *entity, pos_t *pos) {
	void *ih;
    
	aal_assert("umka-942", pos != NULL);
	aal_assert("vpf-037", entity != NULL);

	ih = node40_ih_at(entity, pos->item);

	return node40_len_by_ih(entity, ih, pos, 
				nh_get_num_items(entity));
}


/* Open the node on the given @block with the given key plugin @kplug. Returns
   initialized node instance. */
reiser4_node_t *node40_open(aal_block_t *block, reiser4_key_plug_t *kplug)
{
	reiser4_node_t *entity;
	
	aal_assert("vpf-1415", kplug != NULL);
	aal_assert("vpf-1416", block != NULL);
	
	if (!(entity = node40_prepare(block, kplug)))
		return NULL;

	/* Check the magic. */
	if (nh_get_magic(entity) != NODE40_MAGIC) {
		aal_free(entity);
		return NULL;
	}
	
	return entity;
}

static void node40_get_key_by_ih(reiser4_node_t *entity, void *ih, 
				 reiser4_key_t *key) 
{
	uint32_t size;
	
	key->plug = entity->kplug;
	size = key_size(entity->keypol);
	aal_memcpy(key->body, ih, size);
}

/* Returns key at passed @pos. */
errno_t node40_get_key(reiser4_node_t *entity,
		       pos_t *pos, reiser4_key_t *key)
{
	void *ih;
    
	aal_assert("umka-821", key != NULL);
	aal_assert("umka-939", pos != NULL);
	
	aal_assert("umka-2333", entity != NULL);

	ih = node40_ih_at(entity, pos->item);
		
	/* Allow to fetch the item at pos > item count;
	   but prevent segfault on errors. */
	aal_assert("vpf-1661", ih > entity->block->data);

	node40_get_key_by_ih(entity, ih, key);
	
	return 0;
}

/* Initializes @place at @pos. Fetches all item fields. */
errno_t node40_fetch(reiser4_node_t *entity,
		     pos_t *pos, reiser4_place_t *place)
{
	void *ih;
	
	aal_assert("umka-1813", pos != NULL);
	aal_assert("umka-1602", place != NULL);
	aal_assert("umka-1631", entity != NULL);
	
	ih = node40_ih_at(entity, pos->item);
	
	/* Allow to fetch the item at pos > item count;
	   but prevent segfault on errors. */
	aal_assert("vpf-1660", ih > entity->block->data);

	/* Initializing other fields. */
	place->pos = *pos;
	place->node = entity;
	place->body = node40_ib_by_ih(entity, ih);
	place->len = node40_len_by_ih(entity, ih, pos, 
				      nh_get_num_items(entity));

	/* Getting item key. */
	node40_get_key_by_ih(entity, ih, &place->key);

	/* Initializing item's plugin. */
	place->plug = (reiser4_item_plug_t *)
		node40_core->factory_ops.ifind(ITEM_PLUG_TYPE, 
					       ih_get_pid(ih, entity->keypol));
	
	if (!place->plug) {
		aal_error("Can't find item plugin by its id 0x%x.",
			  ih_get_pid(ih, entity->keypol));
		return -EINVAL;
	}

	/* Init item specific stuff. */
	if (place->plug->balance->init) {
		place->plug->balance->init(place);
	} else {
		/* Zero all item-specific fields here. */
		place->off = 0;
	}
	
	return 0;
}

#ifndef ENABLE_MINIMAL
/* Retutns item overhead for this node format. Widely used in modification and
   estimation routines. */
uint16_t node40_overhead(reiser4_node_t *entity) {
        return ih_size(entity->keypol);
}

/* Returns maximal size of item possible for passed node instance */
static uint16_t node40_maxspace(reiser4_node_t *entity) {
	aal_assert("vpf-016", entity != NULL);
	
	/* Maximal space is node size minus node header and minus item
	   header. */
	return (entity->block->size - sizeof(node40_header_t) -
		ih_size(entity->keypol));
}

/* Calculates size of a region denoted by @pos and @count. This is used by
   node40_copy(), node40_remove(), etc. */
uint32_t node40_size(reiser4_node_t *entity, pos_t *pos, uint32_t count) {
	void *ih;
	uint32_t len;

	aal_assert("umka-3032", pos != NULL);
	aal_assert("umka-3031", entity != NULL);

	ih = node40_ih_at(entity, pos->item);
	
	if (pos->item + count < nh_get_num_items(entity)) {
		uint32_t offset = (ih_size(entity->keypol) * count);
		len = ih_get_offset((ih - offset), entity->keypol);
	} else {
		len = nh_get_free_space_start(entity);
	}

	return len - ih_get_offset(ih, entity->keypol);
}

/* Makes expand passed @node by @len in odrer to make room for insert new
   items/units. This function is used by insert and shift methods. */
errno_t node40_expand(reiser4_node_t *entity, pos_t *pos,
		      uint32_t len, uint32_t count)
{
	void *ih;
	int insert;

	uint32_t item;
	uint32_t items;
	uint32_t offset;
	uint32_t headers;

	aal_assert("vpf-006", pos != NULL);
	aal_assert("umka-817", entity != NULL);

	if (len == 0)
		return 0;
	
	headers = count * ih_size(entity->keypol);

	items = nh_get_num_items(entity);
	insert = (pos->unit == MAX_UINT32);

	/* Getting real pos of the item to be updated. */
	item = pos->item + !insert;
	ih = node40_ih_at(entity, item);

	/* If item pos is inside the range [0..count - 1], we should perform the
	   data moving and offset upadting. */
	if (item < items) {
		void *src, *dst;
		uint32_t i, size;

		/* Moving items bodies */
		offset = ih_get_offset(ih, entity->keypol);
		src = entity->block->data + offset;
		dst = entity->block->data + offset + len;
		size = nh_get_free_space_start(entity) - offset;

		aal_memmove(dst, src, size);

		/* Updating item offsets. */
		for (i = 0; i < items - item; i++) {
			ih_inc_offset(ih, len, entity->keypol);
			ih -= ih_size(entity->keypol);
		}

		/* If this is the insert new item mode, we should prepare the
		   room for new item header and set it up. */
		if (insert) {
			src = node40_ih_at(entity, items - 1);

			dst = node40_ih_at(entity, items - 1 +
					   count);

			size = ih_size(entity->keypol) * (items - item);
			
			aal_memmove(dst, src, size);
		}

		ih = node40_ih_at(entity, item);
	} else {
		offset = nh_get_free_space_start(entity);
	}

	/* Updating node's free space and free space start fields. */
	nh_inc_free_space_start(entity, len);
	nh_dec_free_space(entity, len);

	if (insert) {
                /* Setting up the fields of new item. */
		ih_set_flags(ih, 0, entity->keypol);
		ih_set_offset(ih, offset, entity->keypol);

		/* Setting up node header. */
		nh_inc_num_items(entity, count);
		nh_dec_free_space(entity, headers);
	}

	node40_mkdirty(entity);
	return 0;
}

/* General node40 cutting function. It is used from shift, remove, etc. It
   removes an amount of items specified by @count and shrinks node. */
errno_t node40_shrink(reiser4_node_t *entity, pos_t *pos,
		      uint32_t len, uint32_t count)
{
	uint32_t size;
	void *cur, *end;
	void *src, *dst;

	uint32_t headers;
	uint32_t i, items;

	aal_assert("umka-1800", count > 0);
	aal_assert("umka-1799", pos != NULL);
	aal_assert("umka-1798", entity != NULL);

	items = nh_get_num_items(entity);
	end = node40_ih_at(entity, items - 1);

	if (pos->unit == MAX_UINT32) {
		headers = count * ih_size(entity->keypol);

		/* Moving item header and bodies if it is needed. */
		if (pos->item + count < items) {

			/* Moving item bodies */
			dst = node40_ib_at(entity, pos->item);
			
			src = dst + len; 

			/* Remove operation in broken nodes cannot
			   rely on free_space, do not use node40_size 
			   here. */
			size = end - src;
 
			aal_memmove(dst, src, size);

			/* Moving item headers. */
			src = node40_ih_at(entity, items - 1);
			dst = src + headers;
			
			size = (items - (pos->item + count)) *
				ih_size(entity->keypol);
	
			aal_memmove(dst, src, size);

			/* Updating item offsets. */
			cur = node40_ih_at(entity, pos->item);
	
			for (i = pos->item; i < items - count; i++) {
				ih_dec_offset(cur, len, entity->keypol);
				cur -= ih_size(entity->keypol);
			}
		}

		/* Updating node header */
		nh_dec_num_items(entity, count);
		nh_inc_free_space(entity, (len + headers));
	} else {
		void *ih;
		uint32_t ilen;

		ih = node40_ih_at(entity, pos->item);
		ilen = node40_len_by_ih(entity, ih, pos, items);
		
		/* Moving item bodies */
		src = node40_ib_at(entity, pos->item) + ilen;
		
		/* Remove operation in broken nodes cannot rely on 
		   free_space, use len instead. */

		dst = src - len;

		size = end - src;
		
		aal_memmove(dst, src, size);
		
		/* Updating header offsets */
		cur = ih - ih_size(entity->keypol);
		
		while (cur >= end) {
			ih_dec_offset(cur, len, entity->keypol);
			cur -= ih_size(entity->keypol);
		}

		/* Updating node header and item header */
		nh_inc_free_space(entity, len);
	}

	nh_dec_free_space_start(entity, len);
	node40_mkdirty(entity);
	
	return 0;
}

/* Makes copy of @count items from @src_entity to @dst_entity */
errno_t node40_copy(reiser4_node_t *dst_entity, pos_t *dst_pos,
		    reiser4_node_t *src_entity, pos_t *src_pos,
		    uint32_t count)
{
	void *body;
	uint32_t pol;
	uint32_t size;

	void *ih, *end;
	void *src, *dst;

	uint32_t items;
	uint32_t fss, i;
	uint32_t offset;
	uint32_t headers;

	pol = dst_entity->keypol;
	headers = count * ih_size(pol);
	items = nh_get_num_items(dst_entity);
	fss = nh_get_free_space_start(dst_entity);
	
	size = node40_size(src_entity, src_pos, count);
	
	/* Copying item bodies from src node to dst one. */
	src = node40_ib_at(src_entity, src_pos->item);

	if (dst_pos->item < items - count) {
		body = node40_ib_at(dst_entity, dst_pos->item);
	} else {
		body = dst_entity->block->data + fss - size;
	}
		
	aal_memcpy(body, src, size);

	/* Copying item headers from src node to dst one. */
	src = node40_ih_at(src_entity, src_pos->item +
			   count - 1);

	dst = node40_ih_at(dst_entity, dst_pos->item +
			   count - 1);
			
	aal_memcpy(dst, src, headers);

	/* Updating item headers in dst node. */
	end = node40_ih_at(dst_entity, items - 1);
	ih = dst + (ih_size(pol) * (count - 1));
	
	offset = (body - dst_entity->block->data);
	
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
int64_t node40_modify(reiser4_node_t *entity, pos_t *pos,
		      trans_hint_t *hint, modify_func_t modify_func)
{
        void *ih;
	int64_t res;
        uint32_t pol;
        uint32_t len;
	
        int64_t write;
        reiser4_place_t place;

        len = hint->len + hint->overhead;

	/* Expand node if @len greater than zero. */
	if ((res = node40_expand(entity, pos, len, 1))) {
		aal_error("Can't expand node for insert one "
			  "more item/unit.");
		return res;
	}
        
	pol = entity->keypol;
        ih = node40_ih_at(entity, pos->item);
        
	/* Updating item header if we want to insert new item. */
        if (pos->unit == MAX_UINT32) {
		ih_set_pid(ih, hint->plug->p.id.id, pol);
		aal_memcpy(ih, hint->offset.body, key_size(pol));
        }
        
	/* Preparing place for calling item plugin with them. */
        if (node40_fetch(entity, pos, &place))
		return -EINVAL;

	/* Inserting units into @place. */
	if ((write = modify_func(&place, hint)) < 0) {
		aal_error("Can't insert unit to node %llu, item %u.",
			  (unsigned long long)entity->block->nr,
			  place.pos.item);
		return write;
	}
        
	/* Updating item's key if we insert new item or if we insert unit into
           leftmost postion. */
        if (pos->unit == 0)
                aal_memcpy(ih, place.key.body, key_size(pol));
        	
	return write;
}

errno_t node40_insert(reiser4_node_t *entity,
		      pos_t *pos, trans_hint_t *hint)
{
	modify_func_t ins_func;
	
	aal_assert("umka-2448", pos != NULL);
	aal_assert("umka-1814", hint != NULL);
	aal_assert("umka-818", entity != NULL);

	ins_func = hint->plug->object->insert_units;
	return node40_modify(entity, pos, hint, ins_func);
}

int64_t node40_write(reiser4_node_t *entity, pos_t *pos, trans_hint_t *hint)
{
	modify_func_t write_func;

	aal_assert("umka-2449", pos != NULL);
	aal_assert("umka-2450", hint != NULL);
	aal_assert("umka-2451", entity != NULL);

	write_func = hint->plug->object->write_units;
	return node40_modify(entity, pos, hint, write_func);
}

/* Truncates node at @pos. Needed for tail conversion. */
int64_t node40_trunc(reiser4_node_t *entity, pos_t *pos, trans_hint_t *hint)
{
	void *ih;
	uint32_t pol;
	uint32_t len;
	int64_t count;
	reiser4_place_t place;
	
	aal_assert("umka-2462", pos != NULL);
	aal_assert("umka-2463", entity != NULL);

	pol = entity->keypol;

	/* Getting item at @pos */
	if (node40_fetch(entity, pos, &place))
		return -EINVAL;

	/* Truncating item. */
	count = objcall(&place, object->trunc_units, hint);
	if (count < 0)
		return count;

	len = hint->overhead + hint->len;
	
	/* Shrinking node and and update key. */
	if (len > 0) {
		errno_t res;
		uint32_t number;
		
		number = count;
		place.pos.unit = 0;
		
		if (len >= place.len) {
			/* It seems that len cannot be > than place.len now. */
			number = 1;
			place.pos.unit = MAX_UINT32;
			len = place.len;
		}

		if ((res = node40_shrink(entity, &place.pos,
					 len, number)))
		{
			return res;
		}

		if (len < place.len) {
			ih = node40_ih_at(entity, place.pos.item);
			aal_memcpy(ih, place.key.body, key_size(pol));
		}
	} else {
		ih = node40_ih_at(entity, place.pos.item);
		aal_memcpy(ih, place.key.body, key_size(pol));
	}

	return count;
}

/* This function removes item/unit from the node at specified @pos */
errno_t node40_remove(reiser4_node_t *entity, pos_t *pos,
		      trans_hint_t *hint) 
{
	reiser4_place_t place;
	uint32_t count;
	uint32_t units;
	uint32_t len;
	errno_t res;
	
	aal_assert("umka-987", pos != NULL);
	aal_assert("umka-986", entity != NULL);

	/* Check if we remove some number of whole items, or units inside
	   particular item. */
	if (pos->unit == MAX_UINT32) {
		pos_t walk;
		
		/* Calculating amount of bytes removed item occupie. Node will
		   be shrinked by this value. */
		len = node40_size(entity, pos, hint->count);
		count = hint->count;

		if (hint->region_func) {
			POS_INIT(&walk, pos->item, MAX_UINT32);
				
			/* Calling layout function with @hint->region_func for
			   each removed item in order to let higher levels know
			   that some region is released. */
			for (; walk.item < pos->item + count; walk.item++) {

				if (node40_fetch(entity, &walk, &place))
					return -EINVAL;

				/* Not for nodeprt items. */
				if (place.plug->p.id.group == PTR_ITEM)
					continue;

				/* Only if item has a block layout. */
				if (!place.plug->object->layout)
					continue;
				
				objcall(&place, object->layout, 
					hint->region_func, hint->data);
			}
		}
	} else {
		/* Here we init @count (number of items to be removed) to 1, as
		   here are possible only two cases:

		   (1) Remove item as it get empty (the case when @count is
		   needed).

		   (2) Shrink item at @place.pos.item by @len and @count is
		   ignored.
		*/

		if (node40_fetch(entity, pos, &place))
			return -EINVAL;

		units = objcall(&place, balance->units);
		count = 1;
		
		if (place.plug->object->remove_units) {
			/* Removing units from the item pointed by @pos. */
			if ((res = objcall(&place, object->remove_units, hint)))
				return res;
		}
		
		/* Check if item is empty. If so, we remove it too. */
		if ((len = hint->len + hint->overhead) >= place.len ||
		    units == hint->count)
		{
			/* Forcing node40_shrink() to remove whole item, as we
			   have removed all units from it.*/
			len = place.len;
			pos->unit = MAX_UINT32;
		} else {
			uint32_t pol = entity->keypol;
				
			/* Updating items key if leftmost unit was changed and
			   item will not be removed as it is not yet empty. */
			if (pos->unit == 0) {
				void *ih = node40_ih_at(entity, pos->item);
				aal_memcpy(ih, place.key.body, key_size(pol));
			}
		}
	}

	/* Shrinking node by @hint->len. Item @place.pos will be removed if
	   place.pos points to whole item (unit is MAX_UINT32). */
	return node40_shrink(entity, pos, len, count);
}

/* Fuses two mergeable items if they lie in the same node side by side. This is
   needed for fsck if it discovered, that two items are mergeable and lie in the
   same node (due to some corruption or fail) it will merge them. */
errno_t node40_merge(reiser4_node_t *entity,
		     pos_t *left_pos, pos_t *right_pos)
{
	errno_t res;
	uint32_t pol;
	int32_t delta;
	uint32_t items;
	
	void *left, *right;
	reiser4_place_t left_place;
	reiser4_place_t right_place;
	
	aal_assert("umka-2682", entity != NULL);
	aal_assert("umka-2683", left_pos != NULL);
	aal_assert("umka-2684", right_pos != NULL);

	pol = entity->keypol;
	items = nh_get_num_items(entity);

	aal_assert("umka-2685", (left_pos->item < items &&
				 right_pos->item < items));
	
	/* Check is items lie side by side. */
	delta = left_pos->item - right_pos->item;
	
	if (aal_abs(delta) > 1) {
		aal_error("Can't merge items which lie not side "
			  "by side each other.");
		return -EINVAL;
	}

	/* First stage. Fusing item bodies: we should call some item 
	   method, which will take care about item overhead, etc. */
	if (node40_fetch(entity, left_pos, &left_place)) {
		aal_error("Can't fetch the left item on fusing.");
		return -EINVAL;
	}
	
	if (node40_fetch(entity, right_pos, &right_place)) {
		aal_error("Can't fetch the right item on fusing.");
		return -EINVAL;
	}

	aal_assert("umka-2686", plug_equal(left_place.plug,
					   right_place.plug));

	/* Check if item needs some special actions to merge (like eliminate
	   header). If so, merge items. */
	if (left_place.plug->balance->merge) {
		int32_t space;

		/* Returned space is released space in node and it should be
		   counted in node header. */
		if ((space = objcall(&left_place, balance->merge, 
				     &right_place)))
		{
			right_pos->unit = 0;

			/* Shrink the right item. */
			if ((res = node40_shrink(entity, right_pos, space, 1)))
				return res;
		}
	}
	
	/* The second stage: remove the right item header. */
	left = node40_ih_at(entity, left_pos->item);
	right = node40_ih_at(entity, right_pos->item);

	if (right_pos->item < items - 1) {
		/* Move all headers after right_pos to the right. */
		delta = (items - right_pos->item - 1) * ih_size(pol);
		aal_memmove(left - delta, right - delta, delta);
	}

	nh_dec_num_items(entity, 1);
	nh_inc_free_space(entity, ih_size(pol));

	/* Now make node dirty. */
	node40_mkdirty(entity);
	return 0;
}

/* Updates key at @pos by specified @key */
errno_t node40_set_key(reiser4_node_t *entity, pos_t *pos, reiser4_key_t *key)
{
	void *ih;
	uint32_t key_size;

	aal_assert("umka-819", key != NULL);
    	aal_assert("umka-944", pos != NULL);
	
	aal_assert("umka-811", pos->item <
		   node40_items(entity));

	aal_assert("umka-809", entity != NULL);

	ih = node40_ih_at(entity, pos->item);
	key_size = key_size(entity->keypol);
	aal_memcpy(ih, key->body, key_size);
		
	node40_mkdirty(entity);
	return 0;
}
#endif

#ifdef ENABLE_SHORT_KEYS
/* Helper callback for comparing two keys. This is used by node lookup. */
static int cb_comp_key3(void *ih0, uint32_t pos,
			void *k2, void *data)
{
	reiser4_key_t *key2;
	void *key1;
	
	key1 = ih0 - sizeof(item_header3_t) * pos;
	key2 = (reiser4_key_t *)k2;
	
	return plugcall(key2->plug, compraw, key1, key2->body);
}
#endif

#ifdef ENABLE_LARGE_KEYS
/* Helper callback for comparing two keys. This is used by node lookup. */
static int cb_comp_key4(void *ih0, uint32_t pos,
			void *k2, void *data)
{
	reiser4_key_t *key2;
	void *key1;
	
	/* Key policy is given by @data. */
	key1 = ih0 - sizeof(item_header4_t) * pos;
	key2 = (reiser4_key_t *)k2;
	
	return plugcall(key2->plug, compraw, key1, key2->body);
}
#endif

/* Makes search inside the specified node @entity for @key and stores the result
   into @pos. This function returns 1 if key is found and 0 otherwise. */
lookup_t node40_lookup(reiser4_node_t *entity,
		       lookup_hint_t *hint,
		       lookup_bias_t bias,
		       pos_t *pos)
{
	aux_comp_func_t func;
	void *ih;
	
	aal_assert("umka-478", pos != NULL);
	aal_assert("umka-472", hint != NULL);
	aal_assert("umka-470", entity != NULL);
	aal_assert("umka-3089", hint->key != NULL);
	aal_assert("umka-567", hint->key->body != NULL);

#if defined(ENABLE_SHORT_KEYS) && defined(ENABLE_LARGE_KEYS)
	func = (entity->keypol == 3) ? cb_comp_key3 : cb_comp_key4;
#elif defined(ENABLE_SHORT_KEYS)
	func = cb_comp_key3;
#elif defined(ENABLE_LARGE_KEYS)
	func = cb_comp_key4;
#else
	func = NULL;
#endif
	
	ih = node40_ih_at((reiser4_node_t *)entity, 0);
	switch (aux_bin_search(ih, nh_get_num_items(entity), 
			       hint->key, func, NULL, &pos->item))
	{
	case 1:
		return PRESENT;
	case 0:
		return ABSENT;
	default:
		return -EIO;
	}
}

#ifndef ENABLE_MINIMAL
/* Checks if @place is splittable. */
static int node40_splittable(reiser4_place_t *place, shift_hint_t *hint) {
	/* Check if item's shift_units() and prep_shift() method are
	   implemented. */
	if (!place->plug->balance->shift_units ||
	    !place->plug->balance->prep_shift)
	{
		return 0;
	}
	
	/* We can't shift units from items with one unit. */
	if (!place->plug->balance->units)
		return 0;
	
	return (objcall(place, balance->units) > 0);
}

/* Initializes place by border item data (leftmost or rightmost). */
static errno_t node40_border(reiser4_node_t *entity,
			     int left_border, reiser4_place_t *place)
{
	uint32_t items;
	pos_t pos;
	
	aal_assert("umka-2669", place != NULL);
	aal_assert("umka-2670", entity != NULL);
	
	if ((items = nh_get_num_items(entity)) == 0)
		return -EINVAL;

	POS_INIT(&pos, (left_border ? 0 : items - 1),
		 MAX_UINT32);
	
	return node40_fetch(entity, &pos, place);
}

/* Merges border items of the src and dst nodes. The behavior depends on the
   passed hint pointer. */
static errno_t node40_unite(reiser4_node_t *src_entity,
			    reiser4_node_t *dst_entity, 
			    shift_hint_t *hint,
			    int create)
{
	pos_t pos;
	errno_t res;
	uint32_t pol;
	uint32_t len;
	uint32_t units;

	reiser4_place_t src_place;
	reiser4_place_t dst_place;
	
	uint32_t dst_items;
	uint32_t src_items;
	
	void *src_ih, *dst_ih;
	int left_shift, remove;
	
	aal_assert("umka-1624", hint != NULL);
	aal_assert("umka-1622", src_entity != NULL);
	aal_assert("umka-1623", dst_entity != NULL);
	
	pol = dst_entity->keypol;
	src_items = nh_get_num_items(src_entity);
	dst_items = nh_get_num_items(dst_entity);
	
	hint->units_bytes = node40_space(dst_entity);

	/* Nothing to move or not enough of space to moveat least one byte to
	   @dst_entity. */
	if (src_items == 0 || hint->units_bytes == 0)
		return 0;
	
	left_shift = (hint->control & SF_ALLOW_LEFT);
	
	/* We can't split the leftmost and rightmost items if they are the same
	   insert point points to. */
	if ((hint->control & SF_UPDATE_POINT) &&
	    hint->pos.unit == MAX_UINT32)
	{
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
	if (!node40_splittable(&src_place, hint))
		return 0;
	
	/* Checking if items are mergeable. */
	if (dst_items > 0) {
		/* Getting dst item. */
		if ((res = node40_border(dst_entity, !left_shift, &dst_place)))
			return res;

		/* Check if items has the same flags. If so, they can be tried
		   to be merged. */
		src_ih = node40_ih_at(src_entity, src_place.pos.item);
		dst_ih = node40_ih_at(dst_entity, dst_place.pos.item);

		if (ih_get_flags(src_ih, pol) != ih_get_flags(dst_ih, pol))
			return 0;

		/* Check if we need to create new new item in @dst_entity in
		   order to move data to it. */
		if (left_shift) {
			hint->create = !node40_core->item_ops.mergeable(&dst_place,
									&src_place);
		} else {
			hint->create = !node40_core->item_ops.mergeable(&src_place,
									&dst_place);
		}
	} else {
		/* There are no items in dst node and we create new item in
		   neighbour node anyway. */
		hint->create = 1;
	}

	/* If items are not mergeable and we are in merge mode, we will not
	   create new item in dst node. This mode is needed for mergeing two
	   mergeable items when they lie in different nodes, and in such a way
	   to prevent creating two mergeable items in the same node. */
	if (hint->create != create)
		return 0;
	
	/* Calling item's prep_shift() method in order to estimate how many
	   units may be shifted out. This method also updates unit component of
	   insert point position. After this function is finish @units_bytes
	   will contain real number of bytes to be shifted into neighbour
	   item. */
	if (hint->create) {
		uint32_t overhead;

		/* Getting node overhead in order to substract it from
		   @units_bytes, that is from space allowed to be used. */
		overhead = node40_overhead(dst_entity);

		/* There is not of enough free space in @dst_entity even to
		   create an empty item in it. Getting out. */
		if (hint->units_bytes <= overhead)
			return 0;

		/* Substract node overhead, that is item header. */
		hint->units_bytes -= overhead;

		/* Making estimate how many units and bytes may be shifted. */
		if (objcall(&src_place, balance->prep_shift, NULL, hint))
			return -EINVAL;
		
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
		if (hint->units_number == 0)
			return 0;

		/* Prepare pos new item will be created at. */
		POS_INIT(&pos, (left_shift ? dst_items : 0), MAX_UINT32);
	} else {
		/* The same for case when we will not create new item, but will
		   shift units into existent one in neighbour node. */
		if (objcall(&src_place, balance->prep_shift, &dst_place, hint))
			return -EINVAL;
		
		if (hint->control & SF_UPDATE_POINT &&
		    hint->result & SF_MOVE_POINT)
		{
			hint->pos.item = 0;

			if (left_shift)
				hint->pos.item = dst_items - 1;
		}

		/* Check if shift_units() may shift something at all. If no --
		   getting out of here. */
		if (hint->units_number == 0)
			return 0;

		/* Prepare pos, item will be expanded at. Items are mergeable,
		   so we do not need to create new item in @dst_entity. We just
		   need to expand existent dst item by @hint->units_bytes, thus
		   unit component of @pos is set to 0.*/
		POS_INIT(&pos, (left_shift ? dst_items - 1 : 0), 0);
	}

	/* Expanding node by @hint->units_bytes at @pos. */
	if (node40_expand(dst_entity, &pos, hint->units_bytes, 1)) {
		aal_error("Can't expand node for shifting units into it.");
		return -EINVAL;
	}

	if (hint->create) {
		/* Setting up new item fields such as plugin id and key. */
		dst_ih = node40_ih_at(dst_entity, pos.item);
		ih_set_pid(dst_ih, src_place.plug->p.id.id, pol);
		aal_memcpy(dst_ih, src_place.key.body, key_size(pol));

		/* Copying old item flags to new created one. This is needed,
		   because these flags may say, for instance, that item is
		   already checked by fsck and thus, new item which is created
		   by splitting old one should have the same flags. This is also
		   needed, because items with different flags will not be merged
		   and this will cause bad tree packing. */
		src_ih = node40_ih_at(src_entity, src_place.pos.item);
		ih_set_flags(dst_ih, ih_get_flags(src_ih, pol), pol); 
	}

	/* Initializing @dst_place after that item was expanded by expand()
	   function. */
	if (node40_fetch(dst_entity, &pos, &dst_place))
		return -EINVAL;

	if (hint->create) {
		uint64_t offset;
		
		/* Setting dst item key offset into max key offset of src in
		   order to let item shift method correctly calculate offset
		   of this new item later. */
		objcall(&src_place, balance->maxreal_key, &dst_place.key);
		offset = objcall(&dst_place.key, get_offset);
		objcall(&dst_place.key, set_offset, offset + 1);

	}
	
	/* Getting units number after shift. This is needed to detect correctly,
	   that src item is empty after shift and may be removed. */
	units = objcall(&src_place, balance->units);
	
	/* Shift units from @src_place to @dst_place. */
	if (objcall(&src_place, balance->shift_units, &dst_place, hint)) {
		aal_error("Can't shift units.");
		return -EINVAL;
	}

	/* Set update flag to let high levels code know, that left delemiting
	   keys should be updated. */
	hint->update = 1;
	pos.item = src_place.pos.item;

	/* We will remove src item if it has became empty and insert point does
	   not point it, that is next insert will not be dealing with it. */
	remove = (hint->units_number == units);
	
	/* Updating item's keys after shift_unit() is finished. */
	if (left_shift) {
		/* We do not need to update key of the src item which is going
		   to be removed. */
		if (!remove) {
			src_ih = node40_ih_at(src_entity, src_place.pos.item);
			aal_memcpy(src_ih, src_place.key.body, key_size(pol));
		}
	} else {
		dst_ih = node40_ih_at(dst_entity, dst_place.pos.item);
		aal_memcpy(dst_ih, dst_place.key.body, key_size(pol));
	}
	
	if (remove) {
		/* Like expand() does, shrink() will remove pointed item if unit
		   component is MAX_UINT32 and shrink the item pointed by pos if
		   unit component is not MAX_UINT32. */
		len = src_place.len;
		pos.unit = MAX_UINT32;

		if (pos.item == hint->pos.item)
			hint->pos.unit = MAX_UINT32;
		
		/* As item will be removed, we should update item pos in hint
		   properly. */
		if (!(hint->result & SF_MOVE_POINT) &&
		    pos.item < hint->pos.item)
		{
			hint->pos.item--;
		}
	} else {
		/* Sources item will not be removed, because it is not yet
		   empty, it will be just shrinked by @hint->units_bytes. */
		pos.unit = 0;
		len = hint->units_bytes;
	}

	/* Shrining node by @len. */
	return node40_shrink(src_entity, &pos, len, 1);
}

/* Predicts how many whole item may be shifted from @src_entity to
   @dst_entity. */
static errno_t node40_predict(reiser4_node_t *src_entity,
			      reiser4_node_t *dst_entity, 
			      shift_hint_t *hint)
{
	uint32_t pol;
	uint32_t flags;
	uint32_t space;
	void *cur, *end;
	
	uint32_t src_items;
	uint32_t dst_items;

	dst_items = nh_get_num_items(dst_entity);
	
	if (!(src_items = nh_get_num_items(src_entity)))
		return 0;

	pol = src_entity->keypol;
	space = node40_space(dst_entity);
	
	end = node40_ih_at(src_entity, src_items - 1);

	if (hint->control & SF_ALLOW_LEFT) {
		cur = node40_ih_at(src_entity, 0);
	} else {
		cur = node40_ih_at(src_entity, src_items - 1);
	}
	
	/* Estimating will be finished if @src_items value is exhausted or
	   insert point is shifted out to neighbour node. */
	flags = hint->control;
	
	while (!(hint->result & SF_MOVE_POINT) && src_items > 0) {
		uint32_t len;

		if (!(flags & SF_MOVE_POINT) && (flags & SF_ALLOW_RIGHT)) {
			if (hint->pos.item >= src_items)
				break;
		}
		
		/* Getting length of current item. */
		if (cur == end) {
			len = nh_get_free_space_start(src_entity) -
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
			if (flags & SF_ALLOW_LEFT) {
				if (hint->pos.item == 0) {
					pos_t pos;
					reiser4_place_t place;
					uint32_t units;

					/* If unit component if zero, we can
					   shift whole item pointed by pos. */
					POS_INIT(&pos, 0, MAX_UINT32);
					
					if (node40_fetch(src_entity, &pos, &place))
						return -EINVAL;

					if (!place.plug->balance->units)
						return -EINVAL;
				
					units = objcall(&place, balance->units);

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
		hint->items_number++;
		hint->items_bytes += len;
		src_items--; dst_items++;
		
		space -= (len + node40_overhead(dst_entity));
		
		cur += (flags & SF_ALLOW_LEFT ? -ih_size(pol) :
			ih_size(pol));
	}

	/* After number of whole items was estimated, all free space will be
	   used for estimating how many units may be shifted. */
	hint->units_bytes = space;
	return 0;
}

/* Moves some amount of whole items from @src_entity to @dst_entity */
static errno_t node40_move(reiser4_node_t *src_entity,
			   reiser4_node_t *dst_entity, 
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

	dst_items = nh_get_num_items(dst_entity);
	src_items = nh_get_num_items(src_entity);

	/* Calculating how many items and how many bytes may be moved from
	   @src_entity to @dst_entity. Calculating result is stored in @hint and
	   will be used later. */
	if ((res = node40_predict(src_entity, dst_entity, hint)))
		return res;

	/* No items to be shifted */
	if (hint->items_number == 0 || hint->items_bytes == 0)
		return 0;
	
	/* Initializing src and dst positions, we will used them for moving
	   items. They are initialized in different way for left and right
	   shift. */
	if (hint->control & SF_ALLOW_LEFT) {
		POS_INIT(&src_pos, 0, MAX_UINT32);
		POS_INIT(&dst_pos, dst_items, MAX_UINT32);
	} else {
		POS_INIT(&dst_pos, 0, MAX_UINT32);

		POS_INIT(&src_pos, src_items -
			 hint->items_number, MAX_UINT32);
	}
	
	/* Expanding dst node in order to make room for new items and update
	   node header. */
	if ((res = node40_expand(dst_entity, &dst_pos, hint->items_bytes,
				 hint->items_number)))
	{
		return res;
	}
		
	/* Copying items from src node to dst one */
	if ((res = node40_copy(dst_entity, &dst_pos, src_entity,
			       &src_pos, hint->items_number)))
	{
		return res;
	}

	/* Set update flag to let high levels code know, that left delemiting
	   keys should be updated. */
	hint->update = 1;
	
	/* Shrinking source node after items are copied from it into dst
	   node. */
	return node40_shrink(src_entity, &src_pos, hint->items_bytes,
			     hint->items_number);
}

/* Performs shift of items and units from @src_entity to @dst_entity.

   Shift is performed in three passes:

   (1) This pass is supposed to merge two border items in @src_entity and
   @dst_entity if they are mergeable at all. This is needed to prevent creation
   of mergeable items in the same node during consequent shift of whole items.

   (2) Second pass is supposed to move as many whole items from @src_entity to
   @dst_entity as possible. That is exactly how many how does @dst_entity free
   space allow. This is actually main job of node40_shift() function.

   (3) And finally third pass should again merge two border items in @src_entity
   and @dst_entity after some number of whole items was moved to @dst_entity. It
   is needed to use the rest of space remaining after whole items shift in
   second pass. */
errno_t node40_shift(reiser4_node_t *src_entity,
		     reiser4_node_t *dst_entity,
		     shift_hint_t *hint)
{
	errno_t res;

	aal_assert("umka-2050", src_entity != NULL);
	aal_assert("umka-2051", dst_entity != NULL);

	/* First pass: if merge is allowed, try to merge border items; 
	   otheriwse, check that border items are not mergeable. */
	if (hint->control & SF_ALLOW_MERGE) {
		if ((res = node40_unite(src_entity, dst_entity, hint, 0))) {
			aal_error("Can't merge two nodes during "
				  "node shift operation.");
			return res;
		}
	} else {
		int left_shift;
		reiser4_place_t src_place;
		reiser4_place_t dst_place;
		
		/* Merge is not allowed by @hint->control flags. Check if border
		   items are mergeable. If so, we can't move at least one whole
		   item to @dst_entity, because we have to support all tree
		   invariants and namely there should not be mergeable items in
		   the same node. */
		left_shift = (hint->control & SF_ALLOW_LEFT);

		/* Getting border items and checking if they are mergeable. */
		if ((res = node40_border(src_entity, left_shift, &src_place)))
			return res;

		if ((res = node40_border(dst_entity, !left_shift, &dst_place)))
			return res;
		
		if (node40_core->item_ops.mergeable(&src_place, &dst_place))
			return 0;
	}

	/* Check if insert point is moved to @dst_entity. If so then shift is
	   finished. */
	if (hint->result & SF_MOVE_POINT)
		return 0;

	/* Second pass is started here. Moving some amount of whole items from
	   @src_entity to @dst_entity. */
	if ((res = node40_move(src_entity, dst_entity, hint))) {
		aal_error("Can't move items from node %llu to node %llu.",
			  (unsigned long long)src_entity->block->nr,
			  (unsigned long long)dst_entity->block->nr);
		return res;
	}

	/* Checking if insert point was moved into @dst_entity. If so then shift
	   gets out. */
	if (hint->result & SF_MOVE_POINT)
		return 0;

	/* Third pass is started here. Merges border items with ability to
	   create new item in the @dst_entity. Here our objective is to shift
	   into neighbour node as many units as possible and thus, to fill it
	   up. */
	if (hint->control & SF_ALLOW_MERGE) {
		if ((res = node40_unite(src_entity, dst_entity, hint, 1))) {
			aal_error("Can't unite two nodes during"
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
	    hint->units_number == 0 && hint->create)
	{
		hint->pos.unit = MAX_UINT32;
	}

	return 0;
}

void node40_set_flags(reiser4_node_t *entity, uint32_t pos, uint16_t flags) {
	aal_assert("vpf-1535", node40_items(entity) > pos);
	
	ih_set_flags(node40_ih_at(entity, pos), flags, entity->keypol);
	node40_mkdirty(entity);
}

uint16_t node40_get_flags(reiser4_node_t *entity, uint32_t pos) {
	aal_assert("vpf-1536", node40_items(entity) > pos);
	
	return ih_get_flags(node40_ih_at(entity, pos), entity->keypol);
}

#endif

reiser4_node_plug_t node40_plug = {
	.p = {
		.id    = {NODE_REISER40_ID, 0, NODE_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "node40",
		.desc  = "Node layout plugin.",
#endif
	},
	
	.open		= node40_open,
	.fini		= node40_fini,
	.lookup		= node40_lookup,
	.fetch          = node40_fetch,
	.items		= node40_items,
	
	.get_key	= node40_get_key,
	.get_level	= node40_get_level,
		
#ifndef ENABLE_MINIMAL
	.init		= node40_init,
	.sync           = node40_sync,
	.merge          = node40_merge,

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
	.insert_raw     = node40_insert_raw,
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
	
	.set_flags	= node40_set_flags,
	.get_flags	= node40_get_flags,

	.set_state      = node40_set_state,
	.get_state      = node40_get_state,
	.check_struct	= node40_check_struct
#endif
};

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
