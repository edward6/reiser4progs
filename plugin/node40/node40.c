/*
  node40.c -- reiser4 default node plugin.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#include "node40.h"

extern reiser4_plugin_t node40_plugin;

static reiser4_core_t *core = NULL;

/* Returns item header by pos */
inline item40_header_t *node40_ih_at(aal_block_t *block, int pos) {
	return ((item40_header_t *)(block->data + aal_block_size(block))) - 
		pos - 1;
}

/* Retutrns item body by pos */
inline void *node40_ib_at(aal_block_t *block, int pos) {
	return block->data + ih40_get_offset(node40_ih_at(block, pos));
}

#ifndef ENABLE_COMPACT

static object_entity_t *node40_create(aal_block_t *block, 
				       uint8_t level) 
{
	node40_t *node;
    
	aal_assert("umka-806", block != NULL, return NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
    
	node->block = block;
	node->plugin = &node40_plugin;
    
	/* Plugin setup was moved here because we should support reiser3 */
	nh40_set_pid(nh40(node->block), NODE_REISER40_ID);

	nh40_set_free_space(nh40(node->block), 
			    aal_block_size(node->block) - sizeof(node40_header_t));
    
	nh40_set_free_space_start(nh40(node->block), 
				  sizeof(node40_header_t));
   
	nh40_set_level(nh40(node->block), level);
	nh40_set_magic(nh40(node->block), NODE40_MAGIC);
	nh40_set_num_items(nh40(node->block), 0);

	return (object_entity_t *)node;
}

#endif

static rpid_t node40_pid(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-827", node != NULL, return FAKE_PLUGIN);
	return nh40_get_pid(nh40(node->block));
} 

static object_entity_t *node40_open(aal_block_t *block) {
	node40_t *node;
    
	aal_assert("umka-807", block != NULL, return NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
    
	node->block = block;
	node->plugin = &node40_plugin;
    
	if (nh40_get_pid(nh40(node->block)) != NODE_REISER40_ID) {
		aal_exception_error("Plugin id (%u) does not match current plugin id (%u).", 
				    nh40_get_pid(nh40(node->block)), NODE_REISER40_ID);
		goto error_free_node;
	}

	return (object_entity_t *)node;
    
 error_free_node:
	aal_free(node);
	return NULL;
}

static errno_t node40_close(object_entity_t *entity) {
	aal_assert("umka-825", entity != NULL, return -1);
    
	aal_free(entity);
	return 0;
}

/*
  Confirms that passed node corresponds current plugin. This is something like 
  "probe" method.
*/
static int node40_confirm(aal_block_t *block) {
	aal_assert("vpf-014", block != NULL, return 0);
	return -(nh40_get_magic(nh40(block)) != NODE40_MAGIC);
}

/* Returns item number in given block. Used for any loops through all items */
uint16_t node40_count(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-018", node != NULL, return 0);
	return nh40_get_num_items(nh40(node->block));
}

static errno_t node40_get_key(object_entity_t *entity, 
			      reiser4_pos_t *pos, reiser4_key_t *key) 
{
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-821", key != NULL, return -1);
	aal_assert("vpf-009", node != NULL, return -1);
	aal_assert("umka-939", pos != NULL, return -1);

	aal_assert("umka-810", pos->item < 
		   nh40_get_num_items(nh40(node->block)), return -1);
    
	aal_memcpy(key->body, &(node40_ih_at(node->block, pos->item)->key), 
		   sizeof(key40_t));
    
	return 0;
}

/* Gets item's body at given pos */
static void *node40_item_body(object_entity_t *entity, 
			      reiser4_pos_t *pos)
{
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-040", node != NULL, return NULL);
	aal_assert("umka-940", pos != NULL, return NULL);

	aal_assert("umka-814", pos->item < 
		   nh40_get_num_items(nh40(node->block)), return NULL);
    
	return node40_ib_at(node->block, pos->item);
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
		   nh40_get_num_items(nh40(node->block)), return 0);
    
	return ih40_get_pid(node40_ih_at(node->block, pos->item));
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
		   nh40_get_num_items(nh40(node->block)), return 0);
    
	ih = node40_ih_at(node->block, pos->item);

	free_space_start = nh40_get_free_space_start(nh40(node->block));
    
	return (int)pos->item == node40_count(entity) - 1 ? 
		(int)free_space_start - ih40_get_offset(ih) : 
		(int)ih40_get_offset(ih - 1) - ih40_get_offset(ih);
}

static void node40_item_init(object_entity_t *entity, reiser4_pos_t *pos,
			     item_entity_t *item)
{
	node40_t *node = (node40_t *)entity;
	
	item->context.block = node->block;
	item->context.node = entity;
	
	item->plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE,
					       node40_item_pid(entity, pos));
	
	item->pos = pos->item;
	item->len = node40_item_len(entity, pos);
	item->body = node40_ib_at(node->block, pos->item);
}

#ifndef ENABLE_COMPACT

static errno_t node40_expand(node40_t *node, 
			     reiser4_pos_t *pos, reiser4_item_hint_t *hint) 
{
	void *body;
	int i, item_pos;
	uint16_t offset;
    
	int is_insert;
	int is_space;
	int is_range;

	item40_header_t *ih;
	node40_header_t *nh;

	aal_assert("umka-817", node != NULL, return -1);
	aal_assert("vpf-006", pos != NULL, return -1);
	aal_assert("vpf-007", hint != NULL, return -1);

	is_space = (nh40_get_free_space(nh40(node->block)) >= 
		    hint->len + (pos->unit == ~0ul ? sizeof(item40_header_t) : 0));
    
	is_range = (pos->item <= nh40_get_num_items(nh40(node->block)));
    
	aal_assert("vpf-026", is_space, return -1);
	aal_assert("vpf-027", is_range, return -1);

	is_insert = (pos->unit == ~0ul);
	item_pos = pos->item + !is_insert;
    
	nh = nh40(node->block);
	ih = node40_ih_at(node->block, item_pos);
    
	if (item_pos < nh40_get_num_items(nh)) {
		offset = ih40_get_offset(ih);

		aal_memmove(node->block->data + offset + hint->len, 
			    node->block->data + offset, nh40_get_free_space_start(nh) - offset);
	
		for (i = item_pos; i < nh40_get_num_items(nh); i++, ih--) 
			ih40_set_offset(ih, ih40_get_offset(ih) + hint->len);

		if (is_insert) {
			aal_memmove(ih, ih + 1, sizeof(item40_header_t) * 
				    (nh40_get_num_items(nh) - item_pos));
		}

		ih += (nh40_get_num_items(nh) - item_pos);
	} else
		offset = nh40_get_free_space_start(nh);
    
	nh40_set_free_space(nh, nh40_get_free_space(nh) - 
			    (hint->len + (is_insert ? sizeof(item40_header_t) : 0)));
    
	nh40_set_free_space_start(nh, nh40_get_free_space_start(nh) + hint->len);
    
	if (!is_insert) {
		ih = node40_ih_at(node->block, pos->item);
		ih40_set_len(ih, ih40_get_len(ih) + hint->len);
		return 0;
	}
    
	aal_memcpy(&ih->key, hint->key.body, sizeof(ih->key));
    
	ih40_set_offset(ih, offset);
	ih40_set_pid(ih, hint->plugin->h.sign.id);
	ih40_set_len(ih, hint->len);
    
	return 0;
}

/* Inserts item described by hint structure into node */
static errno_t node40_insert(object_entity_t *entity, 
			     reiser4_pos_t *pos, reiser4_item_hint_t *hint) 
{ 
	item_entity_t item;
	node40_header_t *nh;
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-818", node != NULL, return -1);
	aal_assert("vpf-119", pos != NULL, return -1);
	aal_assert("umka-908", pos->unit == ~0ul, return -1);
    
	if (!hint->data)
		aal_assert("umka-712", hint->key.plugin != NULL, return -1);
    
	if (node40_expand(node, pos, hint))
		return -1;

	nh = nh40(node->block);
	nh40_set_num_items(nh, nh40_get_num_items(nh) + 1);
    
	if (hint->data) {
		aal_memcpy(node40_ib_at(node->block, pos->item), 
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

static errno_t node40_shrink(node40_t *node,
			     reiser4_pos_t *pos, uint16_t len) 
{
	int is_range;
	int is_move;
	int is_cut;
    
	node40_header_t *nh;
	item40_header_t *ih;
	uint16_t offset, ihlen;
        
	aal_assert("umka-958", node != NULL, return -1);
	aal_assert("umka-959", pos != NULL, return -1);

	is_range = (pos->item < nh40_get_num_items(nh40(node->block)));
	aal_assert("umka-960", is_range, return -1);
    
	is_cut = (pos->unit != ~0ul);
    
	nh = nh40(node->block);
	ih = node40_ih_at(node->block, pos->item);
    
	offset = ih40_get_offset(ih);
	ihlen = node40_item_len((object_entity_t *)node, pos);

	is_move = ((offset + ihlen) < nh40_get_free_space_start(nh));
    
	if (is_move) {
		item40_header_t *cur;
		item40_header_t *end;
	
		/* Moving the item bodies */
		aal_memmove(node->block->data + offset, node->block->data + 
			    offset + len, nh40_get_free_space_start(nh) - offset - len);
    
		/* Updating offsets */
		end = node40_ih_at(node->block, nh40_get_num_items(nh) - 1);

		for (cur = ih - 1; cur >= end; cur--)
			ih40_set_offset(cur, ih40_get_offset(cur) - len);
	
		/* Moving headers */
		if (!is_cut)
			aal_memmove(end + 1, end, ((void *)ih) - ((void *)end));
	}
	
	nh40_set_free_space_start(nh, nh40_get_free_space_start(nh) - len);
    
	return 0;
}

/* 
   This function removes item from the node at specified pos. Do not try to 
   understand it. This is impossible. But it works correctly.
*/
errno_t node40_remove(object_entity_t *entity, 
		      reiser4_pos_t *pos) 
{
	uint16_t len;
	item40_header_t *ih;
	node40_header_t *nh;
	node40_t *node = (node40_t *)entity;

	aal_assert("umka-986", node != NULL, return -1);
	aal_assert("umka-987", pos != NULL, return -1);
    
	nh = nh40(node->block);
	ih = node40_ih_at(node->block, pos->item);
	len = node40_item_len((object_entity_t *)node, pos);

	/* Removing either item or unit, depending on pos */
	if (node40_shrink(node, pos, len))
		return -1;
	
	nh40_set_num_items(nh, nh40_get_num_items(nh) - 1);

	nh40_set_free_space(nh, nh40_get_free_space(nh) + len +
			    sizeof(item40_header_t));
	
	return 0;
}

static errno_t node40_cut(object_entity_t *entity, 
			  reiser4_pos_t *pos)
{
	rpid_t pid;
	uint16_t len;
    
	item40_header_t *ih;
	node40_header_t *nh;
    
	item_entity_t item;
	reiser4_plugin_t *plugin;
	node40_t *node = (node40_t *)entity;
	
	aal_assert("umka-988", node != NULL, return -1);
	aal_assert("umka-989", pos != NULL, return -1);
    
	nh = nh40(node->block);
	ih = node40_ih_at(node->block, pos->item);
    
	if ((pid = ih40_get_pid(ih)) == FAKE_PLUGIN)
		return -1;
	
	if (!(plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x.", pid);
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
	nh40_set_free_space(nh, nh40_get_free_space(nh) + len);

	return 0;
}

extern errno_t node40_check(object_entity_t *entity, 
			    uint16_t options);

extern errno_t node40_item_legal(object_entity_t *entity, 
				 reiser4_plugin_t *plugin);
    
#endif

static errno_t node40_valid(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-015", node != NULL, return -1);
    
	if (node40_confirm(node->block))
		return -1;

	return 0;
}

static uint16_t node40_space(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-020", node != NULL, return 0);
    
	return nh40_get_free_space(nh40(node->block));
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
		   nh40_get_num_items(nh40(node->block)), return -1);

	plugin_call(return -1, key->plugin->key_ops, assign,
		    &(node40_ih_at(node->block, pos->item)->key), key->body);
    
	return 0;
}

static errno_t node40_set_level(object_entity_t *entity, uint8_t level) {
	aal_assert("umka-1115", entity != NULL, return -1);
	nh40_set_level(nh40(((node40_t *)entity)->block), level);
	return 0;
}

static errno_t node40_set_stamp(object_entity_t *entity, uint32_t stamp) {
	aal_assert("umka-1126", entity != NULL, return -1);
	nh40_set_mkfs_id(nh40(((node40_t *)entity)->block), stamp);
	return 0;
}

/* 
   Prepare text node description and push it into specied buffer. Caller should
   decide what it should do with filled buffer.
*/
static errno_t node40_print(object_entity_t *entity, char *buff,
			    uint32_t n, uint16_t options) 
{
	aal_assert("vpf-023", entity != NULL, return -1);
	aal_assert("umka-457", buff != NULL, return -1);

	return -1;
}

#endif

static inline void *callback_get_key(void *node, 
				     uint32_t pos, void *data)
{
	item40_header_t *ih = 
		node40_ih_at(((node40_t *)node)->block, pos);

	return &ih->key;
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

	count = nh40_get_num_items(nh40(node->block));
    
	if ((lookup = aux_binsearch(node, count, key->body, callback_get_key, 
				    callback_comp_key, key->plugin, &item)) != -1)
		pos->item = item;

	return lookup;
}

uint8_t node40_get_level(object_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL, return 0);
	return nh40_get_level(nh40(((node40_t *)entity)->block));
}

static uint32_t node40_get_stamp(object_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL, return -1);
	return nh40_get_mkfs_id(nh40(((node40_t *)entity)->block));
}

#ifndef ENABLE_COMPACT

struct node40_estimate {
	node40_t *src;
	node40_t *dst;
    
	reiser4_pos_t *pos;
	shift_flags_t flags;
	
	uint32_t bytes;
	uint32_t items;
	uint32_t part;
};

typedef struct node40_estimate node40_estimate_t;

static errno_t node40_shift_estimate(node40_estimate_t *estimate) {
	uint32_t len, space;
	node40_header_t *nh;
	item40_header_t *end;
	item40_header_t *cur;
	item40_header_t *ins;
	item40_header_t *start;

	nh = nh40(estimate->src->block);
	space = node40_space((object_entity_t *)estimate->dst);
	ins = node40_ih_at(estimate->src->block, estimate->pos->item);

	start = node40_ih_at(estimate->src->block, 0);
	end = node40_ih_at(estimate->src->block, nh40_get_num_items(nh) - 1);
	cur = (estimate->flags & SF_LEFT ? start : end);

	/* Checking if insert point is at end of node */
	if ((int)estimate->pos->item > nh40_get_num_items(nh) - 1)
		return 0;

	while (1) {
		len = (cur == end ? nh40_get_free_space_start(nh) - ih40_get_offset(cur) : 
		       ih40_get_offset(cur) - ih40_get_offset(cur + 1));

		if (!(estimate->flags & SF_MOVIP)) {
			if (cur == ins)
				break;
		} else {
			if ((estimate->flags & SF_LEFT ? (cur > ins) : (cur < ins)))
				break;
		}

		if (space < len)
			break;

		estimate->items++;
		estimate->bytes += len;
		space -= len;

		cur += (estimate->flags & SF_LEFT ? -1 : 1);
	}
	
	estimate->part = space;

	return 0;
}

static int node40_shift(object_entity_t *entity, object_entity_t *target, 
			reiser4_pos_t *pos, shift_flags_t flags)
{
	node40_estimate_t estimate;

	aal_assert("umka-1305", entity != NULL, return -1);
	aal_assert("umka-1306", target != NULL, return -1);
	aal_assert("umka-1307", pos != NULL, return -1);

	aal_memset(&estimate, 0, sizeof(estimate));
    
	estimate.src = (node40_t *)entity;
	estimate.dst = (node40_t *)target;
	estimate.flags = flags;
	estimate.pos = pos;

	if (node40_shift_estimate(&estimate)) {
		aal_exception_error("Can't estimate shift for source node %llu, "
				    "destination node %llu.", aal_block_number(estimate.src->block),
				    aal_block_number(estimate.dst->block));
		return -1;
	}

	/*
	  If after moving the items we will have some amount of free space
	  in destination node, we should try to shift units from the last
	  item to first item of destination node.
	*/
	if (estimate.part > 0) {
	}

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

