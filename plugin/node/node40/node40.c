/*
  node40.c -- reiser4 default node plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "node40.h"

extern reiser4_plugin_t node40_plugin;

static reiser4_core_t *core = NULL;

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

static void node40_item_init(object_entity_t *entity, reiser4_pos_t *pos,
			     item_entity_t *item)
{
	node40_t *node = (node40_t *)entity;
	
	item->context.node = entity;
	item->context.device = node->block->device;
	item->context.blk = aal_block_number(node->block);
	
	item->plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE,
					       node40_item_pid(entity, pos));
	
	item->pos = pos->item;
	item->len = node40_item_len(entity, pos);
	item->body = node40_ib_at(node, pos->item);
}

#ifndef ENABLE_COMPACT

static errno_t node40_expand(node40_t *node, reiser4_pos_t *pos,
			     reiser4_item_hint_t *hint) 
{
	void *body;
	int i, item_pos;
	uint16_t offset;
    
	int is_insert;
	int is_space;
	int is_range;

	item40_header_t *ih;

	aal_assert("umka-817", node != NULL, return -1);
	aal_assert("vpf-006", pos != NULL, return -1);
	aal_assert("vpf-007", hint != NULL, return -1);

	is_space = (nh40_get_free_space(node) >= hint->len +
		    (pos->unit == ~0ul ? sizeof(item40_header_t) : 0));
    
	is_range = (pos->item <= nh40_get_num_items(node));
    
	aal_assert("vpf-026", is_space, return -1);
	aal_assert("vpf-027", is_range, return -1);

	is_insert = (pos->unit == ~0ul);
	item_pos = pos->item + !is_insert;
    
	ih = node40_ih_at(node, item_pos);
    
	if (item_pos < nh40_get_num_items(node)) {
		uint32_t size;
		offset = ih40_get_offset(ih);

		size = nh40_get_free_space_start(node) - offset;
		aal_memmove(node->block->data + offset + hint->len,
			    node->block->data + offset, size);
	
		for (i = item_pos; i < nh40_get_num_items(node); i++, ih--) 
			ih40_set_offset(ih, ih40_get_offset(ih) + hint->len);

		if (is_insert) {
			aal_memmove(ih, ih + 1, sizeof(item40_header_t) * 
				    (nh40_get_num_items(node) - item_pos));
		}

		ih += (nh40_get_num_items(node) - item_pos);
	} else
		offset = nh40_get_free_space_start(node);

	nh40_inc_free_space_start(node, hint->len);
	nh40_dec_free_space(node, (hint->len + (is_insert ? sizeof(item40_header_t) : 0)));
	
	if (!is_insert) {
		ih = node40_ih_at(node, pos->item);
		ih40_set_len(ih, ih40_get_len(ih) + hint->len);
		return 0;
	}
    
	aal_memcpy(&ih->key, hint->key.body, sizeof(ih->key));
    
	ih40_set_offset(ih, offset);
	ih40_set_len(ih, hint->len);
	ih40_set_pid(ih, hint->plugin->h.sign.id);
    
	return 0;
}

/* Inserts item described by hint structure into node */
static errno_t node40_insert(object_entity_t *entity, reiser4_pos_t *pos,
			     reiser4_item_hint_t *hint) 
{ 
	item_entity_t item;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-818", node != NULL, return -1);
	aal_assert("vpf-119", pos != NULL, return -1);
	aal_assert("umka-908", pos->unit == ~0ul, return -1);
    
	if (!hint->data)
		aal_assert("umka-712", hint->key.plugin != NULL, return -1);
    
	if (node40_expand(node, pos, hint))
		return -1;

	nh40_inc_num_items(node, 1);
    
	if (hint->data) {
		aal_memcpy(node40_ib_at(node, pos->item), 
			   hint->data, hint->len);
		return 0;
	}
    
	node40_item_init(entity, pos, &item);
	
	return plugin_call(return -1, hint->plugin->item_ops,
			   init, &item, hint);
}

/* Pastes unit into item described by hint structure. */
static errno_t node40_paste(object_entity_t *entity, reiser4_pos_t *pos,
			    reiser4_item_hint_t *hint) 
{
	item_entity_t item;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-1017", node != NULL, return -1);
	aal_assert("vpf-120", pos != NULL && pos->unit != ~0ul, return -1);

	if (node40_expand(node, pos, hint))
		return -1;

	node40_item_init(entity, pos, &item);
	node40_get_key(entity, pos, &item.key);
	    
	return plugin_call(return -1, hint->plugin->item_ops, 
			   insert, &item, pos->unit, hint);
}

static errno_t node40_shrink(node40_t *node, reiser4_pos_t *pos,
			     uint16_t len) 
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
			ih40_set_offset(cur, ih40_get_offset(cur) - len);
	
		/* Moving headers */
		if (!is_cut)
			aal_memmove(end + 1, end, ((void *)ih) - ((void *)end));
	}
	
	nh40_dec_free_space_start(node, len);
    
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
	nh40_inc_free_space(node, (len + sizeof(item40_header_t)));
	
	return 0;
}

static errno_t node40_cut(object_entity_t *entity, 
			  reiser4_pos_t *pos)
{
	rpid_t pid;
	uint16_t len;
    
	item40_header_t *ih;
    
	item_entity_t item;
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
    
	node40_item_init(entity, pos, &item);
	node40_get_key(entity, pos, &item.key);
	
	if (!(len = plugin_call(return 0, plugin->item_ops, remove, 
				&item, pos->unit)))
		return -1;
	
	if (node40_shrink(node, pos, len))
		return -1;
	
	ih40_set_len(ih, node40_item_len((object_entity_t *)node, pos) - len);
	nh40_inc_free_space(node, len);

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

static char *levels[6] = {
	"LEAF",
	"LEAF",
	"TWIG",
	"INTERNAL",
	"INTERNAL",
	"INTERNAL"
};

static char *groups[6] = {
	"STATDATA ITEM",
	"NODEPTR ITEM",
	"DIRENTRY ITEM",
	"TAIL ITEM",
	"EXTENT ITEM",
	"PERMISSION ITEM",
};

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

		if (core->item_ops.open(&item, entity, &pos)) {
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

struct node40_estimate {
	int ipmoved;
	
	reiser4_pos_t *pos;
	shift_flags_t flags;

	node40_t *src, *dst;
	uint32_t bytes, items, part;
};

typedef struct node40_estimate node40_estimate_t;

static errno_t node40_estimate(node40_estimate_t *estimate) {
	node40_t *node;
	item40_header_t *end;
	item40_header_t *cur;
	item40_header_t *start;

	uint32_t len, dst_space;
	uint32_t src_items, dst_items;

	node = estimate->src;
	src_items = nh40_get_num_items(estimate->src);
	dst_items = nh40_get_num_items(estimate->dst);
	
	start = node40_ih_at(estimate->src, 0);
	end = node40_ih_at(estimate->src, src_items - 1);
	cur = (estimate->flags & SF_LEFT ? start : end);

	dst_space = node40_space((object_entity_t *)estimate->dst);

	while (node == estimate->src) {
		len = (cur == end ? nh40_get_free_space_start(node) - ih40_get_offset(cur) :
		       ih40_get_offset(cur) - ih40_get_offset(cur + 1));

		if (dst_space < (len + sizeof(item40_header_t)))
			break;

		if (!(estimate->flags & SF_MOVIP) && node == estimate->src) {
			if (estimate->flags & SF_LEFT) {
				if (estimate->pos->item == 0)
					break;
			} else {
				if (estimate->pos->item == src_items - 1)
					break;
			}
		}

		if (estimate->flags & SF_LEFT) {
			if (node == estimate->src) {
				if (estimate->pos->item == 0) {
					estimate->pos->item = dst_items;
					node = estimate->dst;
				} else
					estimate->pos->item--;
			}
		} else {
			if (node == estimate->src) {
				if (estimate->pos->item >= src_items - 1) {
					estimate->pos->item = 0;
					node = estimate->dst;

					if (estimate->pos->item > src_items - 1)
						break;
				}
			} else
				estimate->pos->item++;
		}

		src_items--; dst_items++;
		
		estimate->items++;
		estimate->bytes += len;
		dst_space -= (len + sizeof(item40_header_t));

		cur += (estimate->flags & SF_LEFT ? -1 : 1);
	}
	
	estimate->part = dst_space;
	estimate->ipmoved = (node != estimate->src);
	
	return 0;
}

static errno_t node40_shift(object_entity_t *entity, object_entity_t *target, 
			    reiser4_pos_t *pos, shift_hint_t *hint, shift_flags_t flags)
{
	uint32_t i;
	void *dst, *src;
	uint32_t src_items;
	uint32_t dst_items;
	item40_header_t *ih;
	uint32_t headers_size;
	node40_estimate_t estimate;

	aal_assert("umka-1305", entity != NULL, return -1);
	aal_assert("umka-1306", target != NULL, return -1);
	aal_assert("umka-1307", pos != NULL, return -1);
	aal_assert("umka-1579", hint != NULL, return -1);

	aal_memset(&estimate, 0, sizeof(estimate));
    
	estimate.pos = pos;
	estimate.flags = flags;
	estimate.src = (node40_t *)entity;
	estimate.dst = (node40_t *)target;

	/*
	  Estimating shift in order to determine how many items will be shifted,
	  how much bytes, etc.
	*/
	if (node40_estimate(&estimate)) {

		blk_t src_blk = aal_block_number(estimate.src->block);
		blk_t dst_blk = aal_block_number(estimate.dst->block);

		aal_exception_error("Can't estimate shift for source node "
				    "%llu, destination node %llu.", src_blk,
				    dst_blk);
		return -1;
	}

	hint->items = estimate.items;
	hint->bytes = estimate.bytes;
	hint->ipmoved = estimate.ipmoved;
	
	/* Nothing may be shifted */
	if (estimate.items == 0)
		return 0;
	
	dst_items = nh40_get_num_items(estimate.dst);
	src_items = nh40_get_num_items(estimate.src);
	
	headers_size = sizeof(item40_header_t) * estimate.items;

	if (estimate.flags & SF_LEFT) {
		/* Copying item headers from src node to dst */
		src = node40_ih_at(estimate.src, estimate.items - 1);
		dst = node40_ih_at(estimate.dst, dst_items + estimate.items - 1);
			
		aal_memcpy(dst, src, headers_size);

		ih = (item40_header_t *)dst;
		
		/* Copying item bodies from src node to dst */
		src = node40_ib_at(estimate.src, 0);

		dst = estimate.dst->block->data +
			nh40_get_free_space_start(estimate.dst);

		aal_memcpy(dst, src, estimate.bytes);

		/* Updating item headers in dst node */
		for (i = 0; i < estimate.items; i++, ih++) {
			uint32_t offset = nh40_get_free_space_start(estimate.dst);
			ih40_set_offset(ih, offset + (offset - ih40_get_offset(ih)));
		}

		/* Moving src item headers to right place */
		src = node40_ih_at(estimate.src, src_items - 1);
		dst = node40_ih_at(estimate.src, estimate.items - 1);

		aal_memmove(dst, src, (src_items - estimate.items) *
			    sizeof(item40_header_t));

		/* Moving src item bodies to right place */
		ih = node40_ih_at(estimate.src, 0);

		src = estimate.src->block->data + ih40_get_offset(ih);
		dst = estimate.src->block->data + sizeof(node40_header_t);

		aal_memmove(dst, src, nh40_get_free_space_start(estimate.src) -
			    ih40_get_offset(ih));

		/* Updating item headers in src node */
		ih = node40_ih_at(estimate.src, src_items - estimate.items - 1);
		
		for (i = 0; i < src_items - estimate.items; i++, ih++) {
			uint32_t offset = ih40_get_offset(ih);
			ih40_set_offset(ih, offset - estimate.bytes);
		}
	} else {
		/* Preparing space for moving item headers in destination
		 * node */
		if (dst_items > 0) {
			src = node40_ih_at(estimate.dst, dst_items - 1);
			dst = src - headers_size;
		
			aal_memmove(dst, src, headers_size);

			/* Preparing space for moving item bodies in destination
			 * node */
			ih = ((item40_header_t *)dst);
		
			src = estimate.dst->block->data + ih40_get_offset(ih);
			dst = src + estimate.bytes;

			aal_memmove(dst, src, estimate.bytes);

			/* Updating item headers */
			for (i = 0; i < dst_items; i++, ih++) {
				uint32_t offset = ih40_get_offset(ih);
				ih40_set_offset(ih, offset + estimate.bytes);
			}
		}

		/* Copying item headers from src node to dst */
		src = node40_ih_at(estimate.src, src_items - 1);
		dst = node40_ih_at(estimate.dst, estimate.items - 1);

		aal_memcpy(dst, src, headers_size);

		/* Updating item headers in dst node */
		ih = (item40_header_t *)dst;
		
		for (i = 0; i < estimate.items; i++, ih++) {
			uint32_t offset = ih40_get_offset(ih);
			ih40_set_offset(ih, offset - (offset - sizeof(item40_header_t)));
		}
		
		/* Copying item bodies from src node to dst*/
		ih = node40_ih_at(estimate.src, src_items - estimate.items);
		src = estimate.src->block->data + ih40_get_offset(ih);
		dst = estimate.dst->block->data + sizeof(node40_header_t);

		aal_memcpy(dst, src, estimate.bytes);
	}
	
	/* Updating destination node fields */
	nh40_dec_free_space(estimate.dst, (estimate.bytes + headers_size));
	nh40_inc_num_items(estimate.dst, estimate.items);
	nh40_inc_free_space_start(estimate.dst, estimate.bytes);
	
	/* Updating source node fields */
	nh40_inc_free_space(estimate.src, (estimate.bytes + headers_size));
	nh40_dec_num_items(estimate.src, estimate.items);
	nh40_dec_free_space_start(estimate.src, estimate.bytes);

	/*
	  If after moving the items we will have some amount of free space in
	  destination node, we should try to shift units from the last item to
	  first item of destination node.
	*/
	if (estimate.part > 0 && !estimate.ipmoved &&
	    nh40_get_free_space(estimate.dst))
	{
		int mergeable;
		reiser4_plugin_t *plugin;
		reiser4_pos_t src_pos = {0, ~0ul};
		reiser4_pos_t dst_pos = {0, ~0ul};
		item40_header_t *src_ih, *dst_ih;

		item_entity_t src_item;
		item_entity_t dst_item;

		src_items = nh40_get_num_items(estimate.src);
		dst_items = nh40_get_num_items(estimate.dst);

		/* Getting border items from the both nodes */
		if (flags & SF_LEFT) {
			src_pos.item = 0;
			dst_pos.item = dst_items - 1;
			
			src_ih = node40_ih_at(estimate.src, 0);
			dst_ih = node40_ih_at(estimate.dst, dst_items - 1);
		} else {
			src_pos.item = src_items - 1;
			dst_pos.item = 0;
			
			src_ih = node40_ih_at(estimate.src, src_items - 1);
			dst_ih = node40_ih_at(estimate.dst, 0);
		}

		/*
		  Preparing item entities to be passed to item shift
		  method.
		*/
		node40_item_init((object_entity_t *)estimate.src,
				 &src_pos, &src_item);
			
		node40_item_init((object_entity_t *)estimate.dst,
				 &dst_pos, &dst_item);

		mergeable = src_item.plugin->item_ops.mergeable &&
			src_item.plugin->item_ops.mergeable(&src_item, &dst_item);

		if (!(plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, src_ih->pid))) {
			aal_exception_error("Can't find item plugin by its id 0x%x",
					    src_ih->pid);
			return 0;
		}
		
		/* Checking if items are mergeable */
		if (mergeable) {
			if (plugin_call(return -1, plugin->item_ops, shift,
					&dst_item, &src_item, &pos->unit,
					hint, flags))
				return -1;
		} else {
			/*
			  Checking if we have the case when items have realy
			  different nature. Or them have the same type, but
			  belong to the different objects (different files).
			*/
			if (src_item.plugin->h.sign.group !=
			    dst_item.plugin->h.sign.group)
			{
				/*
				  Here we should perform some kind of splitting
				  the border item of the source node onto two
				  parts. The first part will stay in the source
				  node, and another one will be moved to
				  neighbour node.
				*/
			}
		}
	}

	return 0;
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
		.shift		= node40_shift,
		.print		= node40_print,

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
		.shift		= NULL,
		.print		= NULL,
	
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

