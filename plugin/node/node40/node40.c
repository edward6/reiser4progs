/*
  node40.c -- reiser4 default node plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "node40.h"

extern reiser4_plugin_t node40_plugin;

static reiser4_core_t *core = NULL;

static char *levels[6] = {
	"LEAF", "LEAF","TWIG", "INTERNAL", "INTERNAL", "INTERNAL"
};

static char *groups[6] = {
	"STATDATA ITEM", "NODEPTR ITEM", "DIRENTRY ITEM",
	"TAIL ITEM", "EXTENT ITEM", "PERMISSION ITEM"
};

/* Returns item header by pos */
inline item40_header_t *node40_ih_at(node40_t *node, int pos) {
	aal_block_t *block = node->block;

	item40_header_t *ih =
		(item40_header_t *)(block->data + aal_block_size(block));
	
	return ih - pos - 1;
}

/* Retutrns item body by pos */
inline void *node40_ib_at(node40_t *node, int pos) {
	aal_block_t *block = node->block;
	return block->data + ih40_get_offset(node40_ih_at(node, pos));
}

/* Returns node free space end offset */
inline uint16_t node40_free_space_end(node40_t *node) {
	uint32_t items = nh40_get_num_items(node);
	return aal_block_size(node->block) - items * sizeof(item40_header_t);
}

#ifndef ENABLE_COMPACT

static object_entity_t *node40_create(aal_device_t *device, blk_t blk, 
				      uint8_t level)
{
	node40_t *node;
    
	aal_assert("umka-806", device != NULL, return NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
    
	if (!(node->block = aal_block_create(device, blk, 0)))
		goto error_free_node;
	
	node->plugin = &node40_plugin;
    
	nh40_set_pid(node, NODE_REISER40_ID);

	nh40_set_free_space(node, aal_block_size(node->block) -
			    sizeof(node40_header_t));
    
	nh40_set_free_space_start(node, sizeof(node40_header_t));
   
	nh40_set_level(node, level);
	nh40_set_magic(node, NODE40_MAGIC);
	nh40_set_num_items(node, 0);

	return (object_entity_t *)node;
	
 error_free_node:
	aal_free(node);
	return NULL;
}

static errno_t node40_sync(object_entity_t *entity) {
	aal_assert("umka-1552", entity != NULL, return -1);
	return aal_block_sync(((node40_t *)entity)->block);
}

#endif

static rpid_t node40_pid(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-827", node != NULL, return FAKE_PLUGIN);
	return nh40_get_pid(node);
} 

static object_entity_t *node40_open(aal_device_t *device, blk_t blk) {
	node40_t *node;
    
	aal_assert("umka-807", device != NULL, return NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
    
	if (!(node->block = aal_block_open(device, blk))) {
		aal_exception_error("Can't read block %llu. %s.",
				    blk, device->error);
		goto error_free_node;
	}
	
	node->plugin = &node40_plugin;
    
	if (nh40_get_pid(node) != NODE_REISER40_ID) {
		aal_exception_error("Plugin id (%u) does not "
				    "match current plugin id (%u).", 
				    nh40_get_pid(node), NODE_REISER40_ID);
		goto error_free_node;
	}

	return (object_entity_t *)node;
    
 error_free_node:
	aal_free(node);
	return NULL;
}

static errno_t node40_close(object_entity_t *entity) {
	aal_block_t *block;
	
	aal_assert("umka-825", entity != NULL, return -1);

	block = ((node40_t *)entity)->block;
	aal_assert("umka-1578", block != NULL, return -1);

	aal_block_close(block);
	aal_free(entity);
	
	return 0;
}

/* Confirms that passed node corresponds current plugin */
static int node40_confirm(object_entity_t *entity) {
	aal_assert("vpf-014", entity != NULL, return 0);
	return (nh40_get_magic(((node40_t *)entity)) == NODE40_MAGIC);
}

/* Returns item number in given block. Used for any loops through all items */
uint16_t node40_count(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-018", node != NULL, return 0);
	return nh40_get_num_items(node);
}

static errno_t node40_get_key(object_entity_t *entity, reiser4_pos_t *pos,
			      reiser4_key_t *key) 
{
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-821", key != NULL, return -1);
	aal_assert("vpf-009", node != NULL, return -1);
	aal_assert("umka-939", pos != NULL, return -1);

	aal_assert("umka-810", pos->item < 
		   nh40_get_num_items(node), return -1);
    
	aal_memcpy(key->body, &(node40_ih_at(node, pos->item)->key), 
		   sizeof(key40_t));
    
	return 0;
}

/* Gets item's body at given pos */
static void *node40_item_body(object_entity_t *entity, 
			      reiser4_pos_t *pos)
{
	uint32_t items;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-040", node != NULL, return NULL);
	aal_assert("umka-940", pos != NULL, return NULL);

	items = nh40_get_num_items(node);
	aal_assert("umka-814", pos->item < items, return NULL);
    
	return node40_ib_at(node, pos->item);
}

/*
  Retutns items overhead for this node format. Widely used in modification and 
  estimation routines.
*/
static uint16_t node40_overhead(object_entity_t *entity) {
	return sizeof(item40_header_t);
}

/* Returns maximal size of item possible for passed node instance */
static uint16_t node40_maxspace(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-016", node != NULL, return 0);

	return aal_block_size(node->block) - sizeof(node40_header_t) - 
		sizeof(item40_header_t);
}

static rpid_t node40_item_pid(object_entity_t *entity, 
			      reiser4_pos_t *pos)
{
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-039", node != NULL, return FAKE_PLUGIN);
	aal_assert("umka-941", pos != NULL, return FAKE_PLUGIN);

	aal_assert("umka-815", pos->item < 
		   nh40_get_num_items(node), return 0);
    
	return ih40_get_pid(node40_ih_at(node, pos->item));
}

/* Returns length of item at pos */
static uint16_t node40_item_len(object_entity_t *entity, 
				reiser4_pos_t *pos)
{
	item40_header_t *ih;
	uint32_t free_space_start;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-037", node != NULL, return 0);
	aal_assert("umka-942", pos != NULL, return 0);
    
	aal_assert("umka-815", pos->item < 
		   nh40_get_num_items(node), return 0);
    
	ih = node40_ih_at(node, pos->item);
	free_space_start = nh40_get_free_space_start(node);
    
	return (int)pos->item == node40_count(entity) - 1 ? 
		(int)free_space_start - ih40_get_offset(ih) : 
		(int)ih40_get_offset(ih - 1) - ih40_get_offset(ih);
}

static errno_t node40_item_init(item_entity_t *item,
				object_entity_t *entity,
				reiser4_pos_t *pos)
{
	rpid_t pid;
	node40_t *node = (node40_t *)entity;

	aal_assert("umka-1602", item != NULL, return -1);
	aal_assert("umka-1603", pos != NULL, return -1);
	
	item->context.node = entity;
	item->context.device = node->block->device;
	item->context.blk = aal_block_number(node->block);

	pid = node40_item_pid(entity, pos);
	
	if (!(item->plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x",
				    pid);
		return -1;
	}
	
	item->pos = pos->item;
	item->len = node40_item_len(entity, pos);
	item->body = node40_ib_at(node, pos->item);

	if (!(item->key.plugin = core->factory_ops.ifind(KEY_PLUGIN_TYPE,
							 KEY_REISER40_ID)))
	{
		aal_exception_error("Can't find key plugin by its id 0x%x",
				    KEY_REISER40_ID);
		return -1;
	}

	if (node40_get_key(entity, pos, &item->key))
		return -1;

	return 0;
}

static item_entity_t *node40_item_create(object_entity_t *entity,
					 reiser4_pos_t *pos)
{
	item_entity_t *item;

	if (!(item = aal_calloc(sizeof(*item), 0)))
		return NULL;

	if (node40_item_init(item, entity, pos))
		goto error_free_item;

	return item;

 error_free_item:
	aal_free(item);
	return NULL;
}

#ifndef ENABLE_COMPACT

static errno_t node40_expand(node40_t *node, reiser4_pos_t *pos,
			     uint32_t len) 
{
	void *body;
	int is_space;
	int is_range;
	int is_insert;
	int i, item_pos;
	uint16_t offset;

	item40_header_t *ih;

	aal_assert("umka-817", node != NULL, return -1);
	aal_assert("vpf-006", pos != NULL, return -1);

	is_insert = (pos->unit == ~0ul);

	is_space = (nh40_get_free_space(node) >= len +
		    (is_insert ? sizeof(item40_header_t) : 0));
    
	is_range = (pos->item <= nh40_get_num_items(node));
    
	aal_assert("vpf-026", is_space, return -1);
	aal_assert("vpf-027", is_range, return -1);

	item_pos = pos->item + !is_insert;
    
	ih = node40_ih_at(node, item_pos);
    
	if (item_pos < nh40_get_num_items(node)) {
		uint32_t size;
		offset = ih40_get_offset(ih);

		size = nh40_get_free_space_start(node) - offset;
		
		aal_memmove(node->block->data + offset + len,
			    node->block->data + offset, size);
	
		for (i = item_pos; i < nh40_get_num_items(node); i++, ih--) 
			ih40_inc_offset(ih, len);

		if (is_insert) {
			aal_memmove(ih, ih + 1, sizeof(item40_header_t) * 
				    (nh40_get_num_items(node) - item_pos));
		}

		ih += (nh40_get_num_items(node) - item_pos);
	} else
		offset = nh40_get_free_space_start(node);

	nh40_inc_free_space_start(node, len);
	nh40_dec_free_space(node, (len + (is_insert ? sizeof(item40_header_t) : 0)));
	
	if (is_insert) {
		ih40_set_len(ih, len);
		ih40_set_offset(ih, offset);
	} else {
		ih = node40_ih_at(node, pos->item);
		ih40_inc_len(ih, len);
	}
	
	return 0;
}

static errno_t node40_shrink(node40_t *node, reiser4_pos_t *pos,
			     uint32_t len) 
{
	int is_cut;
	int is_move;
	int is_range;
    
	item40_header_t *ih;
	uint16_t offset, ihlen;
        
	aal_assert("umka-958", node != NULL, return -1);
	aal_assert("umka-959", pos != NULL, return -1);

	is_range = (pos->item < nh40_get_num_items(node));
	aal_assert("umka-960", is_range, return -1);
    
	is_cut = (pos->unit != ~0ul);
    
	ih = node40_ih_at(node, pos->item);
    
	offset = ih40_get_offset(ih);
	ihlen = node40_item_len((object_entity_t *)node, pos);

	is_move = ((offset + ihlen) < nh40_get_free_space_start(node));
    
	if (is_move) {
		item40_header_t *cur;
		item40_header_t *end;
	
		/* Moving the item bodies */
		aal_memmove(node->block->data + offset, node->block->data + 
			    offset + len, nh40_get_free_space_start(node) -
			    offset - len);
    
		/* Updating offsets */
		end = node40_ih_at(node, nh40_get_num_items(node) - 1);

		for (cur = ih - 1; cur >= end; cur--)
			ih40_dec_offset(cur, len);
	
		/* Moving headers */
		if (!is_cut)
			aal_memmove(end + 1, end, ((void *)ih) - ((void *)end));
	}
	
	nh40_dec_free_space_start(node, len);
	nh40_inc_free_space(node, (len + !is_cut ? sizeof(item40_header_t) : 0));
    
	return 0;
}

/* Inserts item described by hint structure into node */
static errno_t node40_insert(object_entity_t *entity, reiser4_pos_t *pos,
			     reiser4_item_hint_t *hint) 
{
	item_entity_t item;
	item40_header_t *ih;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-818", node != NULL, return -1);
	aal_assert("vpf-119", pos != NULL, return -1);
	aal_assert("umka-908", pos->unit == ~0ul, return -1);
    
	if (!hint->data)
		aal_assert("umka-712", hint->key.plugin != NULL, return -1);
    
	if (node40_expand(node, pos, hint->len))
		return -1;

	ih = node40_ih_at(node, pos->item);

	ih40_set_pid(ih, hint->plugin->h.sign.id);
	aal_memcpy(&ih->key, hint->key.body, sizeof(ih->key));

	nh40_inc_num_items(node, 1);
    
	if (hint->data) {
		aal_memcpy(node40_ib_at(node, pos->item), 
			   hint->data, hint->len);
		return 0;
	}
    
	if (node40_item_init(&item, entity, pos))
		return -1;
	
	return plugin_call(return -1, hint->plugin->item_ops,
			   init, &item, hint);
}

/* Pastes unit into item described by hint structure. */
static errno_t node40_paste(object_entity_t *entity, reiser4_pos_t *pos,
			    reiser4_item_hint_t *hint) 
{
	item_entity_t item;
	item40_header_t *ih;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-1017", node != NULL, return -1);
	aal_assert("vpf-120", pos != NULL && pos->unit != ~0ul, return -1);

	if (node40_expand(node, pos, hint->len))
		return -1;

	if (node40_item_init(&item, entity, pos))
		return -1;
	
	if (plugin_call(return -1, hint->plugin->item_ops, 
			insert, &item, pos->unit, hint))
		return 0;

	/* Updating left delimiting key */
	if (item.pos == 0 && pos->unit == 0) {
		ih = node40_ih_at(node, item.pos);
		aal_memcpy(&ih->key, item.key.body, sizeof(ih->key));
	}

	return 0;
}

/* This function removes item from the node at specified pos */
errno_t node40_remove(object_entity_t *entity, 
		      reiser4_pos_t *pos) 
{
	uint16_t len;
	item40_header_t *ih;
	node40_t *node = (node40_t *)entity;

	aal_assert("umka-986", node != NULL, return -1);
	aal_assert("umka-987", pos != NULL, return -1);
    
	ih = node40_ih_at(node, pos->item);
	len = node40_item_len((object_entity_t *)node, pos);

	/* Removing either item or unit, depending on pos */
	if (node40_shrink(node, pos, len))
		return -1;
	
	nh40_dec_num_items(node, 1);
	
	return 0;
}

static errno_t node40_cut(object_entity_t *entity, 
			  reiser4_pos_t *pos)
{
	rpid_t pid;
	uint16_t len;
    
	item_entity_t item;
	item40_header_t *ih;
	reiser4_plugin_t *plugin;

	node40_t *node = (node40_t *)entity;
	
	aal_assert("umka-988", node != NULL, return -1);
	aal_assert("umka-989", pos != NULL, return -1);
    
	ih = node40_ih_at(node, pos->item);
    
	if ((pid = ih40_get_pid(ih)) == FAKE_PLUGIN)
		return -1;
	
	if (!(plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its "
				    "id 0x%x.", pid);
		return -1;
	}
    
	if (node40_item_init(&item, entity, pos))
		return -1;
	
	node40_get_key(entity, pos, &item.key);
	
	if (!(len = plugin_call(return -1, plugin->item_ops,
				remove, &item, pos->unit)))
		return -1;
	
	if (node40_shrink(node, pos, len))
		return -1;
	
	ih40_dec_len(ih, len);

	/* Updating left delimiting key */
	if (item.pos == 0 && pos->unit)
		aal_memcpy(&ih->key, item.key.body, sizeof(ih->key));

	return 0;
}

extern errno_t node40_check(object_entity_t *entity);

extern errno_t node40_item_legal(object_entity_t *entity, 
				 reiser4_plugin_t *plugin);
    
#endif

static errno_t node40_valid(object_entity_t *entity) {
	aal_assert("vpf-015", entity != NULL, return -1);
    
	if (node40_confirm(entity))
		return -1;

	return 0;
}

static uint16_t node40_space(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-020", node != NULL, return 0);
    
	return nh40_get_free_space(node);
}

uint8_t node40_get_level(object_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL, return 0);
	return nh40_get_level(((node40_t *)entity));
}

static uint32_t node40_get_stamp(object_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL, return -1);
	return nh40_get_mkfs_id(((node40_t *)entity));
}

#ifndef ENABLE_COMPACT

static errno_t node40_set_key(object_entity_t *entity, 
			      reiser4_pos_t *pos, reiser4_key_t *key) 
{
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-819", key != NULL, return -1);
	aal_assert("umka-820", key->plugin != NULL, return -1);
    
	aal_assert("umka-809", node != NULL, return -1);
	aal_assert("umka-944", pos != NULL, return -1);
    
	aal_assert("umka-811", pos->item < 
		   nh40_get_num_items(node), return -1);

	plugin_call(return -1, key->plugin->key_ops, assign,
		    &(node40_ih_at(node, pos->item)->key), key->body);
    
	return 0;
}

static errno_t node40_set_level(object_entity_t *entity, uint8_t level) {
	aal_assert("umka-1115", entity != NULL, return -1);
	nh40_set_level(((node40_t *)entity), level);
	return 0;
}

static errno_t node40_set_stamp(object_entity_t *entity, uint32_t stamp) {
	aal_assert("umka-1126", entity != NULL, return -1);
	nh40_set_mkfs_id(((node40_t *)entity), stamp);
	return 0;
}

/* 
   Prepare text node description and push it into specied buffer. Caller should
   decide what it should do with filled buffer.
*/
static errno_t node40_print(object_entity_t *entity, aal_stream_t *stream,
			    uint16_t options) 
{
	uint8_t level;
	reiser4_pos_t pos;
	item_entity_t item;

	node40_t *node = (node40_t *)entity;
	
	aal_assert("vpf-023", entity != NULL, return -1);
	aal_assert("umka-457", stream != NULL, return -1);

	level = node40_get_level(entity);
	aal_assert("umka-1580", level > 0, return -1);

	aal_stream_format(stream, "%s NODE (%llu) contains level=%u, "
			  "items=%u, space=%u\n", levels[level],
			  aal_block_number(node->block), level,
			  node40_count(entity), node40_space(entity));
	
	pos.unit = ~0ul;
	
	for (pos.item = 0; pos.item < node40_count(entity); pos.item++) {

		if (node40_item_init(&item, entity, &pos)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, aal_block_number(node->block));
			return -1;
		}

		aal_stream_format(stream, "(%u) ", pos.item);
		aal_stream_format(stream, groups[item.plugin->h.sign.group]);
		aal_stream_format(stream, ": len=%u, KEY: ", item.len);
		
		if (plugin_call(return -1, item.key.plugin->key_ops, print,
				&item.key.body, stream, options))
			return -1;
	
		aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
				  item.plugin->h.sign.id, item.plugin->h.label);

		if (level > LEAF_LEVEL || options) {
			
			if (plugin_call(return -1, item.plugin->item_ops, print,
					&item, stream, options))
				return -1;

			aal_stream_format(stream, "\n");
		}
	}
	
	return 0;
}

#endif

static inline void *callback_get_key(void *node, uint32_t pos,
				     void *data)
{
	return &node40_ih_at((node40_t *)node, pos)->key;
}

static inline int callback_comp_key(void *key1,
				    void *key2, void *data)
{
	aal_assert("umka-566", key1 != NULL, return -1);
	aal_assert("umka-567", key2 != NULL, return -1);
	aal_assert("umka-656", data != NULL, return -1);

	return plugin_call(return -1, ((reiser4_plugin_t *)data)->key_ops, 
			   compare, key1, key2);
}

static int node40_lookup(object_entity_t *entity, 
			 reiser4_key_t *key, reiser4_pos_t *pos)
{
	int lookup; 
	int64_t item;
	uint32_t count;

	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-472", key != NULL, return -1);
	aal_assert("umka-714", key->plugin != NULL, return -1);
    
	aal_assert("umka-478", pos != NULL, return -1);
	aal_assert("umka-470", node != NULL, return -1);

	count = nh40_get_num_items(node);
    
	if ((lookup = aux_binsearch(node, count, key->body, callback_get_key, 
				    callback_comp_key, key->plugin, &item)) != -1)
		pos->item = item;

	return lookup;
}

#ifndef ENABLE_COMPACT

static int node40_mergeable(item_entity_t *item1, item_entity_t *item2) {

	if (!plugin_equal(item1->plugin, item2->plugin))
		return 0;

	return item1->plugin->item_ops.mergeable &&
		item1->plugin->item_ops.mergeable(item1, item2);
}

static errno_t node40_shift_init(object_entity_t *entity,
				 object_entity_t *target, 
				 shift_hint_t *hint)
{
	int mergeable;
	uint32_t units;
	node40_t *src, *dst;
	item40_header_t *cur;
	item40_header_t *end;
	reiser4_plugin_t *plugin;

	uint32_t len, dst_space;
	uint32_t src_items, dst_items;

	reiser4_pos_t src_pos;
	reiser4_pos_t dst_pos;
	
	src = (node40_t *)entity;
	dst = (node40_t *)target;
	
	src_items = nh40_get_num_items(src);
	dst_items = nh40_get_num_items(dst);

	end = node40_ih_at(src, src_items - 1);
	cur = (hint->flags & SF_LEFT ? node40_ih_at(src, 0) : end);

	dst_space = node40_space((object_entity_t *)dst);

	hint->src_item = NULL;
	hint->dst_item = NULL;
	
	/* Predicting how many whole item may be shifted */
	while (src_items > 0) {

		/* Getting length of current item */
		if (cur == end)
			len = nh40_get_free_space_start(src) -
				ih40_get_offset(cur);
		else {
			len = ih40_get_offset(cur) -
				ih40_get_offset(cur + 1);
		}

		/*
		  We go out is there is no enough free space to shift one more
		  whole item.
		*/
		if (dst_space < (len + sizeof(item40_header_t)))
			break;

		/* Updating position and shift hint */
		if (hint->flags & SF_LEFT) {
			if (hint->pos.item == 0)
				break;
			else
				hint->pos.item--;
		} else {
			if (hint->pos.item >= src_items - 1)
				break;
		}

		src_items--;
		dst_items++;
		
		hint->items++;
		hint->bytes += len;
		
		cur += (hint->flags & SF_LEFT ? -1 : 1);
		dst_space -= (len + sizeof(item40_header_t));
	}

	hint->part = dst_space;
				
	if (src_items == 0 && hint->part == 0)
		return 0;
	
	/* Predicting how many units may be shifted */
	if (hint->flags & SF_LEFT) {
		if (hint->pos.item == 0) {
			/*
			  Initializing items to be examaned by the predict method of
			  corresponding item plugin.
			*/
			src_pos.unit = ~0ul;
			src_pos.item = hint->items;
		
			hint->src_item = node40_item_create(
				(object_entity_t *)src, &src_pos);

			if (hint->items > 0) {
				dst_pos.unit = ~0ul;
				dst_pos.item = hint->items - 1;
		
				hint->dst_item = node40_item_create(
					(object_entity_t *)src, &dst_pos);
			} else {
				dst_pos.unit = ~0ul;
				dst_pos.item = dst_items - 1;
		
				hint->dst_item = node40_item_create(
					(object_entity_t *)dst, &dst_pos);
			}

			units = plugin_call(return -1, hint->src_item->plugin->item_ops,
					    count, hint->src_item);
		
			/*
			  Checking if we can shift one more whole item. This is
			  possible when we have enough free space (and we have)
			  and if we are permitted to shift insert point too.
			*/
			if ((hint->pos.unit >= units || hint->pos.unit == ~0ul ||
			     units <= 1) && (hint->flags & SF_MOVIP))
			{
				hint->items++;
				hint->bytes += len;
				hint->pos.item = dst_items;
				
				return 0;
			}
				
			if (!hint->src_item->plugin->item_ops.predict)
				return 0;

			/* Checking if items are mergeable */
			if ((mergeable = node40_mergeable(hint->src_item, hint->dst_item))) {
				if (hint->src_item->plugin->item_ops.predict(
					    hint->src_item, hint->dst_item, hint))
					return -1;
			} else {
				hint->part -= sizeof(item40_header_t);

				if (hint->src_item->plugin->item_ops.predict(
					    hint->src_item, NULL, hint))
					return -1;
			}

			if (hint->flags & SF_MOVIP)
				hint->pos.item = 0;
		} else
			hint->flags &= ~SF_MOVIP;
	} else {
		/* Initializing src item */
		src_pos.unit = ~0ul;
		src_pos.item = nh40_get_num_items(src) - hint->items - 1;
		
		hint->src_item = node40_item_create(
			(object_entity_t *)src, &src_pos);

		units = plugin_call(return -1, hint->src_item->plugin->item_ops,
				    count, hint->src_item);

		/*
		  Checking if we are going to insert new item/unit among/into
		  existent one.
		*/
		if (hint->pos.item <= src_items - 1) {
			
			if ((hint->pos.unit == 0 || hint->pos.unit == ~0ul ||
			    units <= 1) && (hint->flags & SF_MOVIP))
			{
				hint->items++;
				hint->bytes += len;
				hint->pos.item = 0;
				
				return 0;
			}

			/* Initializing dst item */
			if (src_items - hint->items < src_items) {
				dst_pos.unit = ~0ul;
				dst_pos.item = src_items - hint->items;
		
				hint->dst_item = node40_item_create(
					(object_entity_t *)src, &dst_pos);
			} else {
				if (dst_items > 0) {
					dst_pos.unit = ~0ul;
					dst_pos.item = 0;
		
					hint->dst_item = node40_item_create(
						(object_entity_t *)dst, &dst_pos);
				}
			}

			/* Calling predict method from the item plugin */
			if (!hint->src_item->plugin->item_ops.predict)
				return 0;

			if ((mergeable = hint->dst_item != NULL &&
			     node40_mergeable(hint->src_item, hint->dst_item)))
			{
				if (hint->src_item->plugin->item_ops.predict(
					    hint->src_item, hint->dst_item, hint))
					return -1;
			} else {
				hint->part -= sizeof(item40_header_t);

				if (hint->src_item->plugin->item_ops.predict(
					    hint->src_item, NULL, hint))
					return -1;
			}

			if (hint->flags & SF_MOVIP)
				hint->pos.item = 0;
		} else {
			/*
			  We was going to insert new item at the end of node,
			  and now it will be inserted at position zero.
			*/
			hint->pos.item = 0;
			hint->flags |= SF_MOVIP;

			aal_assert("umka-1601", hint->pos.unit == ~0ul, return -1);
		}
	}

	return 0;
}

static void node40_shift_fini(shift_hint_t *hint) {
	if (hint->src_item)
		aal_free(hint->src_item);

	if (hint->dst_item)
		aal_free(hint->dst_item);
}

static errno_t node40_shift(object_entity_t *entity,
			    object_entity_t *target, 
			    shift_hint_t *hint)
{
	int mergeable;
	void *dst, *src;
	uint32_t i, size;
	uint32_t src_items;
	uint32_t dst_items;

	item40_header_t *ih;
	uint32_t headers_size;

	node40_t *src_node;
	node40_t *dst_node;
	
	aal_assert("umka-1305", entity != NULL, return -1);
	aal_assert("umka-1306", target != NULL, return -1);
	aal_assert("umka-1579", hint != NULL, return -1);

	src_node = (node40_t *)entity;
	dst_node = (node40_t *)target;

	dst_items = nh40_get_num_items(dst_node);
	src_items = nh40_get_num_items(src_node);

	/*
	  Estimating shift in order to determine how many items will be shifted,
	  how much bytes, etc.
	*/
	if (node40_shift_init(entity, target, hint)) {

		blk_t src_blk = aal_block_number(src_node->block);
		blk_t dst_blk = aal_block_number(dst_node->block);

		aal_exception_error("Can't predict shift for source node "
				    "%llu, destination node %llu.", src_blk,
				    dst_blk);
		return -1;
	}

	aal_assert("umka-1590", hint->items <= src_items, goto out);
	
	/* Nothing should be shifted */
	if (hint->items == 0 || hint->bytes == 0)
		goto shift_units;
	
	headers_size = sizeof(item40_header_t) * hint->items;

	if (hint->flags & SF_LEFT) {
		/* Copying item headers from src node to dst */
		src = node40_ih_at(src_node, hint->items - 1);
		dst = node40_ih_at(dst_node, dst_items + hint->items - 1);
			
		aal_memcpy(dst, src, headers_size);

		ih = (item40_header_t *)dst;
		
		/* Copying item bodies from src node to dst */
		src = node40_ib_at(src_node, 0);

		dst = dst_node->block->data +
			nh40_get_free_space_start(dst_node);

		aal_memcpy(dst, src, hint->bytes);

		/* Updating item headers in dst node */
		for (i = 0; i < hint->items; i++, ih++) {
			uint32_t offset = nh40_get_free_space_start(dst_node);
			ih40_set_offset(ih, offset + (offset - ih40_get_offset(ih)));
		}

		if (src_items > hint->items) {
			/* Moving src item headers to right place */
			src = node40_ih_at(src_node, src_items - 1);
			dst = node40_ih_at(src_node, hint->items - 1);

			aal_memmove(dst, src, (src_items - hint->items) *
				    sizeof(item40_header_t));

			/* Moving src item bodies to right place */
			ih = node40_ih_at(src_node, 0);

			src = src_node->block->data + ih40_get_offset(ih);
			dst = src_node->block->data + sizeof(node40_header_t);

			aal_memmove(dst, src, nh40_get_free_space_start(src_node) -
				    ih40_get_offset(ih));

			/* Updating item headers in src node */
			ih = node40_ih_at(src_node, src_items - hint->items - 1);
		
			for (i = 0; i < src_items - hint->items; i++, ih++)
				ih40_dec_offset(ih, hint->bytes);
		}
	} else {
		uint32_t offset;
		
		/* Preparing space for headers in dst node */
		if (dst_items > 0) {
			src = node40_ih_at(dst_node, dst_items - 1);
			dst = src - headers_size;

			size = dst_items * sizeof(item40_header_t);
			
			aal_memmove(dst, src, size);

			/* Updating item headers */
			ih = (item40_header_t *)dst;
			
			for (i = 0; i < dst_items; i++, ih++)
				ih40_inc_offset(ih, hint->bytes);
			
			/* Preparing space for bodies in dst node */
			src = dst_node->block->data + sizeof(node40_header_t);
			dst = src + hint->bytes;

			size = nh40_get_free_space_start(dst_node) -
				sizeof(node40_header_t);
			
			aal_memmove(dst, src, size);
		}

		/* Copying item headers from src node to dst */
		src = node40_ih_at(src_node, src_items - 1);
		dst = node40_ih_at(dst_node, hint->items - 1);

		aal_memcpy(dst, src, headers_size);

		/* Updating item headers in dst node */
		offset = nh40_get_free_space_start(src_node) -
			hint->bytes;
		
		ih = node40_ih_at(dst_node, 0);
		
		for (i = 0; i < hint->items; i++, ih--)
			ih40_dec_offset(ih, (offset - sizeof(node40_header_t)));

		/* Copying item bodies from src node to dst */
		ih = node40_ih_at(src_node, src_items - 1) +
			(hint->items - 1);
		
		src = src_node->block->data + ih40_get_offset(ih);
		dst = dst_node->block->data + sizeof(node40_header_t);

		aal_memcpy(dst, src, hint->bytes);
	}
	
	/* Updating destination node fields */
	nh40_dec_free_space(dst_node, (hint->bytes + headers_size));
	nh40_inc_num_items(dst_node, hint->items);
	nh40_inc_free_space_start(dst_node, hint->bytes);
	
	/* Updating source node fields */
	nh40_inc_free_space(src_node, (hint->bytes + headers_size));
	nh40_dec_num_items(src_node, hint->items);
	nh40_dec_free_space_start(src_node, hint->bytes);

 shift_units:
	/*
	  If after moving the items we still having some amount of free space in
	  destination node, we should try to shift units from the last item to
	  first item of destination node.
	*/
	if (hint->units == 0 || hint->part == 0)
		goto out;

	src_items = nh40_get_num_items(src_node);
	dst_items = nh40_get_num_items(dst_node);

	if (!hint->src_item->plugin->item_ops.count)
		goto out;
	
	if (hint->src_item->plugin->item_ops.count(hint->src_item) <= 1)
		goto out;
	
	/* Checking if items are mergeable */
	mergeable = hint->dst_item && node40_mergeable(hint->src_item,
						       hint->dst_item);

	/* Checking if items are mergeable */
	if (mergeable) {
		reiser4_pos_t pos;

		if (hint->flags & SF_LEFT) {
			pos.item = dst_items - 1;
			
			pos.unit = hint->dst_item->plugin->item_ops.count(
				hint->dst_item);
		} else {
			pos.item = 0;
			pos.unit = 0;
		}

		if (node40_expand(dst_node, &pos, hint->part)) {
			aal_exception_error("Can't expand item for "
					    "shifting units into it.");
			goto error_shift_fini;
		}
		
		ih = node40_ih_at(dst_node, pos.item);

		if (hint->dst_item)
			node40_item_init(hint->dst_item,
					 (object_entity_t *)dst_node, &pos);
		else 
			hint->dst_item = node40_item_create(
				(object_entity_t *)dst_node, &pos);
	} else {
		reiser4_pos_t pos;

		if (hint->flags & SF_LEFT) {
			pos.item = dst_items;
			
			pos.unit = hint->dst_item->plugin->item_ops.count(
				hint->dst_item);
		} else {
			pos.item = 0;
			pos.unit = ~0ul;
		}
		
		if (node40_expand(dst_node, &pos, hint->part)) {
			aal_exception_error("Can't expand node for "
					    "shifting units into it.");
			goto error_shift_fini;
		}
		nh40_inc_num_items(dst_node, 1);
		
		ih = node40_ih_at(dst_node, pos.item);
		ih40_set_pid(ih, hint->src_item->plugin->h.sign.id);
		aal_memcpy(&ih->key, hint->src_item->key.body, sizeof(ih->key));

		if (hint->dst_item)
			node40_item_init(hint->dst_item,
					 (object_entity_t *)dst_node, &pos);
		else 
			hint->dst_item = node40_item_create(
				(object_entity_t *)dst_node, &pos);
	}

	if (plugin_call(goto error_shift_fini, hint->src_item->plugin->item_ops,
			shift, hint->src_item, hint->dst_item, hint))
		goto error_shift_fini;

	aal_memcpy(&ih->key, hint->dst_item->key.body, sizeof(ih->key));

	/* Updating src item length */
	ih = node40_ih_at(src_node, hint->src_item->pos);
	ih40_dec_len(ih, hint->part);

	/* Updating source node fields */
	nh40_inc_free_space(src_node, hint->part);
	nh40_dec_free_space_start(src_node, hint->part);

 out:
	node40_shift_fini(hint);
	return 0;
	
 error_shift_fini:
	node40_shift_fini(hint);
	return -1;
}

#endif

static reiser4_plugin_t node40_plugin = {
	.node_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = NODE_REISER40_ID,
				.group = 0,
				.type = NODE_PLUGIN_TYPE
			},
			.label = "node40",
			.desc = "Node for reiserfs 4.0, ver. " VERSION,
		},
		.open		= node40_open,
		.close		= node40_close,
	
		.confirm	= node40_confirm,
		.valid		= node40_valid,
	
		.lookup		= node40_lookup,
		.count		= node40_count,
	
		.overhead	= node40_overhead,
		.maxspace	= node40_maxspace,
		.space		= node40_space,
		.pid		= node40_pid,
	
		.get_key	= node40_get_key,
		.get_level	= node40_get_level,
		.get_stamp	= node40_get_stamp,
	
#ifndef ENABLE_COMPACT
		.create		= node40_create,
		.sync           = node40_sync,
		.insert		= node40_insert,
		.remove		= node40_remove,
		.paste		= node40_paste,
		.cut		= node40_cut,
		.check		= node40_check,
		.print		= node40_print,
		.shift		= node40_shift,

		.set_key	= node40_set_key,
		.set_level	= node40_set_level,
		.set_stamp	= node40_set_stamp,
	
		.item_legal	= node40_item_legal,
#else
		.create		= NULL,
		.sync           = NULL,
		.insert		= NULL,
		.remove		= NULL,
		.paste		= NULL,
		.cut		= NULL,
		.check		= NULL,
		.print		= NULL,
		.shift		= NULL,
	
		.set_key	= NULL,
		.set_level	= NULL,
		.set_stamp	= NULL,
	
		.item_legal	= NULL,
#endif
		.item_len	= node40_item_len,
		.item_body	= node40_item_body,
		.item_pid	= node40_item_pid
	}
};

static reiser4_plugin_t *node40_start(reiser4_core_t *c) {
	core = c;
	return &node40_plugin;
}

plugin_register(node40_start, NULL);

