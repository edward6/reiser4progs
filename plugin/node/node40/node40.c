/*
  node40.c -- reiser4 default node plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "node40.h"

extern reiser4_plugin_t node40_plugin;

static reiser4_core_t *core = NULL;

/* Names of levels nodes lie on. It is used for node40_print function */
static char *levels[6] = {
	"LEAF", "LEAF","TWIG", "INTERNAL", "INTERNAL", "INTERNAL"
};

/* Names of item groups. Used by node40_print too */
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

/* Creates node40 entity on specified device and block with specified level */
static object_entity_t *node40_create(aal_device_t *device, blk_t blk, 
				      uint8_t level)
{
	node40_t *node;
    
	aal_assert("umka-806", device != NULL, return NULL);

	/* Allocating memory for the entity */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	/* Creating block, node will lie in */
	if (!(node->block = aal_block_create(device, blk, 0)))
		goto error_free_node;
	
	node->plugin = &node40_plugin;

	/* Setting up node header */
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

/* Saves node to device */
static errno_t node40_sync(object_entity_t *entity) {
	aal_assert("umka-1552", entity != NULL, return -1);
	return aal_block_sync(((node40_t *)entity)->block);
}

#endif

/* Returns node plugin id */
static rpid_t node40_pid(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-827", node != NULL, return INVAL_PID);
	return nh40_get_pid(node);
} 

/* Opens node on passed device and block number */
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
	return (object_entity_t *)node;
    
 error_free_node:
	aal_free(node);
	return NULL;
}

/* Closes node by means of closing its block */
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

/*
  Returns item number in passed node entity. Used for any loops through the all
  node items.
*/
uint16_t node40_items(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-018", node != NULL, return 0);
	return nh40_get_num_items(node);
}

/* Returns key at passed @pos */
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
    
	aal_assert("vpf-016", node != NULL, return 0);

	/* Blocksize minus node header and minus item overhead */
	return aal_block_size(node->block) - sizeof(node40_header_t) - 
		sizeof(item40_header_t);
}

/* Gets item's body at passed @pos */
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

/* Returns item plugin id at specified @pos */
static rpid_t node40_item_pid(object_entity_t *entity, 
			      reiser4_pos_t *pos)
{
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-039", node != NULL, return INVAL_PID);
	aal_assert("umka-941", pos != NULL, return INVAL_PID);

	aal_assert("umka-815", pos->item < 
		   nh40_get_num_items(node), return 0);
    
	return ih40_get_pid(node40_ih_at(node, pos->item));
}

/* Returns length of item at pos. */
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

	/*
	  Item length is next item body offset minus current item offset. If we
	  are on the last item then we use free space start for that. We use
	  this formula, because reiser4 kernel code does not set item's length
	  correctly.
	*/
	ih = node40_ih_at(node, pos->item);
	free_space_start = nh40_get_free_space_start(node);

	if (pos->item == (uint32_t)(node40_items(entity) - 1))
		return free_space_start - ih40_get_offset(ih);

	return ih40_get_offset(ih - 1) - ih40_get_offset(ih);
}

/* Initializes item entity in order to pass it to a item plugin routine */
static errno_t node40_item(item_entity_t *item,
			   node40_t *node,
			   uint32_t pos)
{
	rpid_t pid;
	reiser4_pos_t item_pos;

	aal_assert("umka-1602", item != NULL, return -1);
	aal_assert("umka-1631", node != NULL, return -1);

	item_pos.unit = ~0ul;
	item_pos.item = pos;
	
	/* Initializes item's context (node, device, block number, etc) */
	item->con.device = node->block->device;
	item->con.blk = aal_block_number(node->block);

	/* Initializing item's plugin */
	pid = node40_item_pid((object_entity_t *)node, &item_pos);
	
	if (!(item->plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find item plugin by its id 0x%x",
				    pid);
		return -1;
	}
	
	/* Initializing item's pos, body pointer and length */
	item->pos = pos;
	item->body = node40_ib_at(node, pos);
	item->len = node40_item_len((object_entity_t *)node, &item_pos);

	/* Initializing item's key */
	if (!(item->key.plugin = core->factory_ops.ifind(KEY_PLUGIN_TYPE,
							 KEY_REISER40_ID)))
	{
		aal_exception_error("Can't find key plugin by its id 0x%x",
				    KEY_REISER40_ID);
		return -1;
	}

	if (node40_get_key((object_entity_t *)node, &item_pos, &item->key))
		return -1;

	return 0;
}

#ifndef ENABLE_COMPACT

/*
  Makes expand passed node by passed @len in odrer to insert new item/unit into
  it. This function is used by insert, paste and shift methods.
*/
static errno_t node40_expand(node40_t *node,
			     reiser4_pos_t *pos,
			     uint32_t len)
{
	int is_space;
	int is_range;
	int is_insert;
	int i, item_pos;
	uint32_t offset;

	item40_header_t *ih;

	aal_assert("umka-817", node != NULL, return -1);
	aal_assert("vpf-006", pos != NULL, return -1);

	/* Checks for input validness */
	is_insert = (pos->unit == ~0ul);

	is_space = (nh40_get_free_space(node) >= len +
		    (is_insert ? sizeof(item40_header_t) : 0));
    
	is_range = (pos->item <= nh40_get_num_items(node));
    
	aal_assert("vpf-026", is_space, return -1);
	aal_assert("vpf-027", is_range, return -1);

	/* Getting real pos of the item to be updated */
	item_pos = pos->item + !is_insert;
	ih = node40_ih_at(node, item_pos);

	/*
	  If item pos is inside the range [0..count - 1], we should perform
	  the data moving and offset upadting.
	*/
	if (item_pos < nh40_get_num_items(node)) {
		uint32_t size;
		void *src, *dst;

		offset = ih40_get_offset(ih);

		src = node->block->data + offset;
		dst = node->block->data + offset + len;
		size = nh40_get_free_space_start(node) - offset;

		/* Moving items bodies */
		aal_memmove(dst, src, size);

		/* Updating item offsets */
		for (i = item_pos; i < nh40_get_num_items(node); i++, ih--) 
			ih40_inc_offset(ih, len);

		/*
		  If this is the insert new item mode, we should prepare the
		  room for ne witem header and set it up.
		*/
		if (is_insert) {
			aal_memmove(ih, ih + 1, sizeof(item40_header_t) * 
				    (nh40_get_num_items(node) - item_pos));
		}

		ih += (nh40_get_num_items(node) - item_pos);
	} else
		offset = nh40_get_free_space_start(node);

	/* Updating node's free space and free space start fields */
	nh40_inc_free_space_start(node, len);
	nh40_dec_free_space(node, len);

	if (is_insert) {

                /* Setting up the fields of new item */
		ih40_set_len(ih, len);
		ih40_set_offset(ih, offset);

		/* Setting up node free space start */
		nh40_dec_free_space(node, sizeof(item40_header_t));

		/* Initializing new item's body */
		aal_memset(node->block->data + offset, 0, len);
	} else {

		/* Increasing item len mfor the case of pasting new units */
		ih = node40_ih_at(node, pos->item);
		ih40_inc_len(ih, len);
	}
	
	return 0;
}

/* Makes shrink node by passed @len after item/unit was removed */
static errno_t node40_shrink(node40_t *node,
			     reiser4_pos_t *pos,
			     uint32_t len) 
{
	int is_range;
    
	uint32_t offset;
	uint32_t item_len;
	item40_header_t *ih;

	/* Checking input for validness */
	aal_assert("umka-958", node != NULL, return -1);
	aal_assert("umka-959", pos != NULL, return -1);

	is_range = (pos->item < nh40_get_num_items(node));
	aal_assert("umka-960", is_range, return -1);
    
	ih = node40_ih_at(node, pos->item);
    
	offset = ih40_get_offset(ih);
	item_len = node40_item_len((object_entity_t *)node, pos);

	/*
	  If we are going to remove not the last item, we need update item
	  bodies and them offsets.
	*/
	if ((offset + item_len) < nh40_get_free_space_start(node)) {
		uint32_t size;
		void *src, *dst;
		
		item40_header_t *cur;
		item40_header_t *end;
	
		/* Moving item bodies */
		src = node->block->data + offset + item_len;
		dst = src - len;
		
		size = nh40_get_free_space_start(node) - offset - len;
		
		aal_memmove(dst, src, size);
    
		/* Updating item offsets after bodies moving */
		end = node40_ih_at(node, nh40_get_num_items(node) - 1);

		for (cur = ih - 1; cur >= end; cur--)
			ih40_dec_offset(cur, len);
	
		/* Moving item headers if this is the item remove case */
		if (pos->unit == ~0ul)
			aal_memmove(end + 1, end, (void *)ih - (void *)end);
	}

	/* Updating item header in the case of cutting */
	if (pos->unit != ~0ul)
		ih40_dec_len(ih, len);

	/* Updating node header */
	nh40_dec_free_space_start(node, len);
	nh40_inc_free_space(node, len);

	/*
	  Increasing free space by item overhead in the case of removing whole
	  item from the node.
	*/
	if (pos->unit == ~0ul)
		nh40_inc_free_space(node, sizeof(item40_header_t));
    
	return 0;
}

/* Inserts item described by hint structure into node */
static errno_t node40_insert(object_entity_t *entity, reiser4_pos_t *pos,
			     reiser4_item_hint_t *hint) 
{
	item_entity_t item;
	item40_header_t *ih;
	
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-119", pos != NULL, return -1);
	aal_assert("umka-818", node != NULL, return -1);
	aal_assert("umka-908", pos->unit == ~0ul, return -1);
    
	if (!hint->data)
		aal_assert("umka-712", hint->key.plugin != NULL, return -1);

	/* Makes expand of the node new item will be inaserted to */
	if (node40_expand(node, pos, hint->len))
		return -1;

	/* Updating item header of the new item */
	ih = node40_ih_at(node, pos->item);
	ih40_set_pid(ih, hint->plugin->h.id);
	aal_memcpy(&ih->key, hint->key.body, sizeof(ih->key));

	nh40_inc_num_items(node, 1);

	/*
	  If item hint contains some data, we just copy it and going out. This
	  mode probably will be used by fsck.
	*/
	if (hint->data) {
		aal_memcpy(node40_ib_at(node, pos->item), 
			   hint->data, hint->len);
		return 0;
	}

	/* Preparing item for calling item plugin with them */
	if (node40_item(&item, node, pos->item))
		return -1;

	/* Calling item plugin to perform initializing the item. */
	return plugin_call(return -1, hint->plugin->item_ops,
			   init, &item, hint);
}

/* Inserts a unit into item described by hint structure. */
static errno_t node40_paste(object_entity_t *entity, reiser4_pos_t *pos,
			    reiser4_item_hint_t *hint) 
{
	item_entity_t item;
	item40_header_t *ih;
	
	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-1017", node != NULL, return -1);
	aal_assert("vpf-120", pos != NULL && pos->unit != ~0ul, return -1);

	/* Expanding item at @pos to insert new unit(s) into it */
	if (node40_expand(node, pos, hint->len))
		return -1;

	/* Initilizing item entity to pass it to item plugin */
	if (node40_item(&item, node, pos->item))
		return -1;

	/* Calling insert method of the item plugin */
	if (plugin_call(return -1, hint->plugin->item_ops, 
			insert, &item, pos->unit, hint))
		return 0;

	/* Updating left delimiting key */
	if (pos->item == 0 && pos->unit == 0) {
		ih = node40_ih_at(node, item.pos);
		aal_memcpy(&ih->key, item.key.body, sizeof(ih->key));
	}

	return 0;
}

/* This function removes item from the node at specified @pos */
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

	/* Removing item or unit, depending on @pos */
	if (node40_shrink(node, pos, len))
		return -1;
	
	nh40_dec_num_items(node, 1);
	
	return 0;
}

/* Cut the unit at specified @pos */
static errno_t node40_cut(object_entity_t *entity, 
			  reiser4_pos_t *pos)
{
	uint32_t len;
	item_entity_t item;
	item40_header_t *ih;

	node40_t *node = (node40_t *)entity;
	
	aal_assert("umka-988", node != NULL, return -1);
	aal_assert("umka-989", pos != NULL, return -1);
    
	ih = node40_ih_at(node, pos->item);

	/* Initializing item entity */
	if (node40_item(&item, node, pos->item))
		return -1;

	/*
	  Removing unit at pos->unit. This function returns how much bytes were
	  released by removing. It is needed for correct shrinking the node
	  after operation complete.
	*/
	if (!(len = plugin_call(return -1, item.plugin->item_ops,
				remove, &item, pos->unit)))
		return -1;

	/* Shrinking node by @len */
	if (node40_shrink(node, pos, len))
		return -1;

	/* Updating left delimiting key */
	if (pos->item == 0 && pos->unit == 0)
		aal_memcpy(&ih->key, item.key.body, sizeof(ih->key));

	return 0;
}

extern errno_t node40_check(object_entity_t *entity);

#endif

/* Checks node for validness */
static errno_t node40_valid(object_entity_t *entity) {
	aal_assert("vpf-015", entity != NULL, return -1);
    
	if (node40_confirm(entity))
		return -1;

	return 0;
}

/* Returns node free space */
static uint16_t node40_space(object_entity_t *entity) {
	node40_t *node = (node40_t *)entity;
    
	aal_assert("vpf-020", node != NULL, return 0);
    
	return nh40_get_free_space(node);
}

/* Returns node level */
uint8_t node40_get_level(object_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL, return 0);
	return nh40_get_level(((node40_t *)entity));
}

/* Returns node stamp */
static uint32_t node40_get_stamp(object_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL, return -1);
	return nh40_get_mkfs_id(((node40_t *)entity));
}

#ifndef ENABLE_COMPACT

/* Updates key at @pos by specified @key */
static errno_t node40_set_key(object_entity_t *entity, 
			      reiser4_pos_t *pos,
			      reiser4_key_t *key) 
{
	node40_t *node = (node40_t *)entity;

	/* Checking input on validness */
	aal_assert("umka-819", key != NULL, return -1);
	aal_assert("umka-820", key->plugin != NULL, return -1);
    
	aal_assert("umka-809", node != NULL, return -1);
	aal_assert("umka-944", pos != NULL, return -1);
    
	aal_assert("umka-811", pos->item < 
		   nh40_get_num_items(node), return -1);

	/* Calling key plugin assign method */
	plugin_call(return -1, key->plugin->key_ops, assign,
		    &(node40_ih_at(node, pos->item)->key), key->body);
    
	return 0;
}

/* Updating node level */
static errno_t node40_set_level(object_entity_t *entity, uint8_t level) {
	aal_assert("umka-1115", entity != NULL, return -1);
	nh40_set_level(((node40_t *)entity), level);
	return 0;
}

/* Updating node stamp */
static errno_t node40_set_stamp(object_entity_t *entity, uint32_t stamp) {
	aal_assert("umka-1126", entity != NULL, return -1);
	nh40_set_mkfs_id(((node40_t *)entity), stamp);
	return 0;
}

/* Prepare text node description and push it into specied @stream. */
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
			  node40_items(entity), node40_space(entity));
	
	pos.unit = ~0ul;

	/* Loop through the all items */
	for (pos.item = 0; pos.item < node40_items(entity); pos.item++) {

		if (node40_item(&item, node, pos.item)) {
			aal_exception_error("Can't open item %u in node %llu.", 
					    pos.item, aal_block_number(node->block));
			return -1;
		}

		aal_stream_format(stream, "(%u) ", pos.item);
		aal_stream_format(stream, groups[item.plugin->h.group]);
		aal_stream_format(stream, ": len=%u, KEY: ", item.len);
		
		if (plugin_call(return -1, item.key.plugin->key_ops, print,
				&item.key.body, stream, options))
			return -1;
	
		aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
				  item.plugin->h.id, item.plugin->h.label);

		/* Printing item by means of calling item print method */
		if (plugin_call(return -1, item.plugin->item_ops, print,
				&item, stream, options))
			return -1;

		aal_stream_format(stream, "\n");
	}
	
	return 0;
}

/* 
  This checks the level constrains like no internal and extent items at leaf
  level or no statdata items at internal level.  Returns 0 is legal, 1 - not,
  -1 - error.
*/
errno_t node40_item_legal(object_entity_t *entity,
			  reiser4_plugin_t *plugin)
{
	uint8_t level;
	node40_t *node = (node40_t *)entity;

	aal_assert("vpf-225", node != NULL, return -1);
	aal_assert("vpf-237", plugin != NULL, return -1);
    
	level = node40_get_level(entity);
    
	if (plugin->h.group == NODEPTR_ITEM) {
		if (level == NODE40_LEAF)
			return 1;
	} else if (plugin->h.group == EXTENT_ITEM) {
		if (level != NODE40_TWIG)
			return 1;
	} else if (level != NODE40_LEAF) 
		return 1;
    
	return 0;
}

#endif

/* Helper callback for comparing two keys. Thios is used by node lookup */
static inline int callback_comp_key(void *node, uint32_t pos,
				    void *key2, void *data)
{
	void *key1;
	aal_assert("umka-566", node != NULL, return -1);
	aal_assert("umka-567", key2 != NULL, return -1);
	aal_assert("umka-656", data != NULL, return -1);

	key1 = &node40_ih_at((node40_t *)node, pos)->key;
	
	return plugin_call(return -1, ((reiser4_plugin_t *)data)->key_ops, 
			   compare, key1, key2);
}

/*
  Makes search inside the specified node @entity for @key and stores the result
  into @pos. This function returns 1 if key is found and 0 otherwise.
*/
static int node40_lookup(object_entity_t *entity, 
			 reiser4_key_t *key, reiser4_pos_t *pos)
{
	int lookup; 
	int64_t item;
	uint32_t items;

	node40_t *node = (node40_t *)entity;
    
	aal_assert("umka-472", key != NULL, return -1);
	aal_assert("umka-714", key->plugin != NULL, return -1);
    
	aal_assert("umka-478", pos != NULL, return -1);
	aal_assert("umka-470", node != NULL, return -1);

	items = nh40_get_num_items(node);

	/* Calling binsearch with local callbacks in order to find needed key */
	if ((lookup = aux_binsearch(node, items, key->body, callback_comp_key, 
				    key->plugin, &item)) != -1)
		pos->item = item;

	return lookup;
}

#ifndef ENABLE_COMPACT

/* Checks if two item entities are mergeable */
static int node40_mergeable(item_entity_t *item1, item_entity_t *item2) {

	if (!plugin_equal(item1->plugin, item2->plugin))
		return 0;

	return item1->plugin->item_ops.mergeable &&
		item1->plugin->item_ops.mergeable(item1, item2);
}

/*
  Merges neighbours items from the different nodes if they are mergeable. It is
  needed for producing the tree wich will do not confuse the kernel code.
*/
static errno_t node40_merge_items(node40_t *src_node,
				  node40_t *dst_node, 
				  shift_hint_t *hint)
{
	int mergeable;
	reiser4_pos_t pos;
	uint32_t src_items;
	uint32_t dst_items;
	item40_header_t *ih;

	item_entity_t src_item;
	item_entity_t dst_item;

	aal_assert("umka-1627", hint != NULL, return -1);
	aal_assert("umka-1625", src_node != NULL, return -1);
	aal_assert("umka-1626", dst_node != NULL, return -1);

	if (!(src_items = nh40_get_num_items(src_node)))
		return 0;

	/*
	  We can't merge items if they contain insert point. This is the job for
	  node40_shift_items function.
	*/
	if (hint->flags & SF_LEFT) {
		if (hint->pos.item == 0)
			return 0;
	} else {
		if (hint->pos.item >= src_items - 1)
			return 0;
	}
	
	if (!(dst_items = nh40_get_num_items(dst_node)))
		return 0;

	/* Initializing src and dst item entities */
	node40_item(&src_item, src_node,
		    (hint->flags & SF_LEFT ? 0 : src_items - 1));
			
	if (!src_item.plugin->item_ops.predict)
		return 0;

	node40_item(&dst_item, dst_node, (hint->flags & SF_LEFT ?
					  dst_items - 1 : 0));

	/* Check if items are mergeable */
	if (!node40_mergeable(&src_item, &dst_item))
		return 0;

	hint->items = 1;
	hint->bytes = src_item.len;
	hint->part = nh40_get_free_space(dst_node);

	if (src_item.plugin->item_ops.predict(&src_item, &dst_item, hint))
		return -1;

	if (hint->bytes > nh40_get_free_space(dst_node))
		return 0;

	/* Expanding dst node to make room for units to be moved to it */
	pos.unit = 0;
	pos.item = dst_item.pos;

	if (node40_expand(dst_node, &pos, hint->part)) {
		aal_exception_error("Can't expand item for "
				    "shifting units into it.");
		return -1;
	}

	/*
	  Reinitializing dst item after it was expanded by node40_expand
	  function.
	*/
	node40_item(&dst_item, dst_node, pos.item);

	/* Calling item method shift */
	if (plugin_call(return -1, src_item.plugin->item_ops, shift,
			&src_item, &dst_item, hint))
		return -1;

	/* Updating item key */
	if (hint->flags & SF_LEFT) {
		ih = node40_ih_at(src_node, pos.item);
		aal_memcpy(&ih->key, src_item.key.body, sizeof(ih->key));
	} else {
		ih = node40_ih_at(dst_node, pos.item);
		aal_memcpy(&ih->key, dst_item.key.body, sizeof(ih->key));
	}

	/* Updating source node fields */
	pos.item = src_item.pos;
	pos.unit = src_items <= hint->units ? ~0ul : 0;
	
	return node40_shrink(src_node, &pos, hint->bytes);
}

/*
  Estimates how many units may be shifted from src_node to dst_node. The result
  is stored inside @hint.
*/
static errno_t node40_predict_units(node40_t *src_node,
				    node40_t *dst_node, 
				    shift_hint_t *hint)
{
	int mergeable;
	uint32_t dst_items;
	uint32_t src_items;
	item_entity_t src_item;
	item_entity_t dst_item;

	aal_assert("umka-1624", hint != NULL, return -1);
	aal_assert("umka-1622", src_node != NULL, return -1);
	aal_assert("umka-1623", dst_node != NULL, return -1);
	
	src_items = nh40_get_num_items(src_node);
	dst_items = nh40_get_num_items(dst_node);
	
	if (src_items == 0 || hint->part == 0)
		return 0;
	
	aal_memset(&src_item, 0, sizeof(src_item));
	aal_memset(&dst_item, 0, sizeof(dst_item));
	
	/*
	  Initializing items to be examaned by the predict method of
	  corresponding item plugin.
	*/
	node40_item(&src_item, src_node,
		    (hint->flags & SF_LEFT ? 0 : src_items - 1));
			
	if (!src_item.plugin->item_ops.predict)
		return 0;

	/* Checking if items are mergeable */
	mergeable = dst_items > 0;

	if (dst_items > 0) {
		node40_item(&dst_item, dst_node,
			    (hint->flags & SF_LEFT ? dst_items - 1 : 0));
		
		mergeable = node40_mergeable(&src_item, &dst_item);
	}

	/* Calling predict method of the src and dst items */
	if (!src_item.plugin->item_ops.predict)
		return 0;

	if (mergeable) {
		if (src_item.plugin->item_ops.predict(&src_item, &dst_item, hint))
			return -1;
	} else {
		uint32_t overhead = node40_overhead((object_entity_t *)dst_node);
		
		/*
		  In the case items are not mergeable, we need count also item
		  overhead, because new item will be created.
		*/

		if (hint->part < overhead)
			return 0;
		
		hint->part -= overhead;
		
		if (src_item.plugin->item_ops.predict(&src_item, NULL, hint))
			return -1;
	}

	/* Updating insert point position if it was moved into neighbour item */
	if (hint->flags & SF_MOVIP)
		hint->pos.item = (hint->flags & SF_LEFT ? dst_items - 1 : 0);
		
	return 0;
}

/*
  Makes shift units from src_node to dst_node, basing on @hint previously
  estimated.
*/
static errno_t node40_shift_units(node40_t *src_node,
				  node40_t *dst_node, 
				  shift_hint_t *hint)
{
	int mergeable;
	reiser4_pos_t pos;
	uint32_t src_items;
	uint32_t dst_items;
	item40_header_t *ih;

	item_entity_t src_item;
	item_entity_t dst_item;

	aal_assert("umka-1627", hint != NULL, return -1);
	aal_assert("umka-1625", src_node != NULL, return -1);
	aal_assert("umka-1626", dst_node != NULL, return -1);
	
	/*
	  If after moving the items we still having some amount of free space in
	  destination node, we should try to shift units from the last item to
	  first item of destination node.
	*/
	if (hint->units == 0 || hint->part == 0)
		return 0;

	/* Getting item number from the src node and dst one */
	src_items = nh40_get_num_items(src_node);
	dst_items = nh40_get_num_items(dst_node);

	aal_memset(&src_item, 0, sizeof(src_item));
	aal_memset(&dst_item, 0, sizeof(dst_item));

	/* Initializing src item entity */
	if (hint->flags & SF_LEFT)
		node40_item(&src_item, src_node, 0);
	else
		node40_item(&src_item, src_node, src_items - 1);
	
	mergeable = dst_items > 0;
	
	/* Initializing dst item entity */
	if (dst_items > 0) {
		if (hint->flags & SF_LEFT)
			node40_item(&dst_item, dst_node, dst_items - 1);
		else
			node40_item(&dst_item, dst_node, 0);

		mergeable = node40_mergeable(&src_item, &dst_item);
	}
	
	/* We can't shift units from items with one unit */
	if (!src_item.plugin->item_ops.units)
		return 0;
	
	if (src_item.plugin->item_ops.units(&src_item) <= 1)
		return 0;
	
	/*
	  If items mergeable, we should expand dst_node for inserting units,
	  otherwise for inserting new item.
	*/
	if (mergeable) {
		pos.item = hint->flags & SF_LEFT ?
			dst_items - 1 : 0;
		
		pos.unit = 0;

		if (node40_expand(dst_node, &pos, hint->part)) {
			aal_exception_error("Can't expand item for "
					    "shifting units into it.");
			return -1;
		}

		/*
		  Reinitializing dst item after it was expanded by node40_expand
		  function.
		*/
		node40_item(&dst_item, dst_node, pos.item);
	} else {
		pos.item = hint->flags & SF_LEFT ? dst_items : 0;
		pos.unit = ~0ul;
		
		if (node40_expand(dst_node, &pos, hint->part)) {
			aal_exception_error("Can't expand node for "
					    "shifting units into it.");
			return -1;
		}
		
		nh40_inc_num_items(dst_node, 1);

		/* Setting up new item fiedls */
		ih = node40_ih_at(dst_node, pos.item);
		ih40_set_pid(ih, src_item.plugin->h.id);
		aal_memcpy(&ih->key, src_item.key.body, sizeof(ih->key));

		/*
		  Initializing dst item after it was created by node40_expand
		  function.
		*/
		node40_item(&dst_item, dst_node, pos.item);
	}

	/* Calling item method shift */
	if (plugin_call(return -1, src_item.plugin->item_ops, shift,
			&src_item, &dst_item, hint))
		return -1;

	if (hint->flags & SF_LEFT) {
		/* Updating src_node left delimiting key */
		ih = node40_ih_at(src_node, src_item.pos);
		aal_memcpy(&ih->key, src_item.key.body, sizeof(ih->key));
	} else {
		/* Updating dst_node left delimiting key */
		ih = node40_ih_at(dst_node, dst_item.pos);
		aal_memcpy(&ih->key, dst_item.key.body, sizeof(ih->key));
	}
	
	/* Updating source node fields */
	pos.unit = 0;
	pos.item = src_item.pos;

	return node40_shrink(src_node, &pos, hint->part);
}

/*
  Estimatuing how many whole items may be shifted from the src node to dst
  one. This function is called before item shifting.
*/
static errno_t node40_predict_items(node40_t *src_node,
				    node40_t *dst_node, 
				    shift_hint_t *hint)
{
	uint32_t len;
	uint32_t space;

	uint32_t overhead;
	uint32_t src_items;
	uint32_t dst_items;

	shift_flags_t flags;
	item40_header_t *cur;
	item40_header_t *end;

	aal_assert("umka-1620", hint != NULL, return -1);
	aal_assert("umka-1621", src_node != NULL, return -1);
	aal_assert("umka-1619", dst_node != NULL, return -1);
	
	if (!(src_items = nh40_get_num_items(src_node)))
		return 0;

	dst_items = nh40_get_num_items(dst_node);
	
	end = node40_ih_at(src_node, src_items - 1);
	cur = (hint->flags & SF_LEFT ? node40_ih_at(src_node, 0) : end);

	space = node40_space((object_entity_t *)dst_node);

	flags = hint->flags;
	hint->flags &= ~SF_MOVIP;

	overhead = node40_overhead((object_entity_t *)dst_node);

	while (src_items > 0 && !(hint->flags & SF_MOVIP)) {

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
		if (space < len + overhead)
			break;

		/* Updating insert position */
		if (flags & SF_LEFT) {
			if (hint->pos.item == 0) {
				uint32_t units;
				item_entity_t item;

				node40_item(&item, src_node, 0);

				if (!item.plugin->item_ops.units)
					return -1;
				
				units = item.plugin->item_ops.units(&item);

				/*
				  Breaking if insert point reach the end of
				  node.
				*/
				if (flags & SF_MOVIP &&
				    (hint->pos.unit == ~0ul ||
				     hint->pos.unit >= units - 1))
				{
					hint->flags |= SF_MOVIP;
					hint->pos.item = dst_items;
				} else
					break;
				
			} else
				hint->pos.item--;
		} else {
			/*
			  Checking if insert point reach the end of node. Hint
			  is updated here.
			*/
			if (hint->pos.item >= src_items - 1) {
				
				if (hint->pos.item == src_items - 1) {

					if (flags & SF_MOVIP &&
					    (hint->pos.unit == ~0ul ||
					     hint->pos.unit == 0))
					{
						hint->flags |= SF_MOVIP;
						hint->pos.item = 0;
					} else {
						if (hint->pos.unit != ~0ul)
							break;
					}
				} else {
					if (flags & SF_MOVIP) {
						hint->flags |= SF_MOVIP;
						hint->pos.item = 0;
					}
					break;
				}
			}
		}

		/* Updating item counters and hint */
		src_items--;
		dst_items++;
		
		hint->items++;
		hint->bytes += len;

		space -= (len + overhead);
		cur += (flags & SF_LEFT ? -1 : 1);
	}

	/*
	  After number of whole items was estimated, all free space will be
	  used for estimating how many units may be shifted.
	*/
	hint->part = space;

	return 0;
}

/* Makes shift of items from the src node to dst one */
static errno_t node40_shift_items(node40_t *src_node,
				  node40_t *dst_node, 
				  shift_hint_t *hint)
{
	void *dst, *src;
	uint32_t offset;
	uint32_t i, size;
	uint32_t src_items;
	uint32_t dst_items;

	item40_header_t *ih;
	uint32_t headers_size;
	
	aal_assert("umka-1305", src_node != NULL, return -1);
	aal_assert("umka-1306", dst_node != NULL, return -1);
	aal_assert("umka-1579", hint != NULL, return -1);

	/* No items to be shifted */
	if (hint->items == 0 || hint->bytes == 0)
		return 0;
	
	dst_items = nh40_get_num_items(dst_node);
	src_items = nh40_get_num_items(src_node);

	headers_size = sizeof(item40_header_t) * hint->items;

	if (hint->flags & SF_LEFT) {
		/* Copying item headers from src node to dst */
		src = node40_ih_at(src_node, hint->items - 1);
		dst = node40_ih_at(dst_node, (dst_items + hint->items - 1));
			
		aal_memcpy(dst, src, headers_size);

		ih = (item40_header_t *)dst;
		
		/* Copying item bodies from src node to dst */
		src = node40_ib_at(src_node, 0);

		dst = dst_node->block->data +
			nh40_get_free_space_start(dst_node);

		aal_memcpy(dst, src, hint->bytes);

		offset = nh40_get_free_space_start(dst_node);
		
		/* Updating item headers in dst node */
		for (i = 0; i < hint->items; i++, ih++)
			ih40_inc_offset(ih, (offset - sizeof(node40_header_t)));

		if (src_items > hint->items) {
			/* Moving src item headers to right place */
			src = node40_ih_at(src_node, src_items - 1);
			dst = src + headers_size;

			aal_memmove(dst, src, (src_items - hint->items) *
				    sizeof(item40_header_t));

			/* Moving src item bodies to right place */
			src = src_node->block->data + sizeof(node40_header_t) +
				hint->bytes;
			
			dst = src_node->block->data + sizeof(node40_header_t);

			aal_memmove(dst, src, nh40_get_free_space_start(src_node) -
				    hint->bytes);

			/* Updating item headers in src node */
			ih = node40_ih_at(src_node, src_items - hint->items - 1);
		
			for (i = 0; i < src_items - hint->items; i++, ih++)
				ih40_dec_offset(ih, hint->bytes);
		}
	} else {
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
		ih = node40_ih_at(dst_node, 0);
		offset = nh40_get_free_space_start(src_node) - hint->bytes;

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

	return 0;
}

/* Performs shift of items and units from @entity to @neighbour */
static errno_t node40_shift(object_entity_t *entity,
			    object_entity_t *neighb,
			    shift_hint_t *hint)
{
	shift_hint_t merge;
	shift_flags_t flags;

	node40_t *src_node = (node40_t *)entity;
	node40_t *dst_node = (node40_t *)neighb;

	hint->bytes = 0;
	hint->items = 0;
	merge = *hint;

/*	if (node40_merge_items(src_node, dst_node, &merge))
		return -1;*/
	
	flags = hint->flags;
	
	/*
	  Estimating shift in order to determine how many items will be shifted,
	  how much bytes, etc.
	*/
	if (node40_predict_items(src_node, dst_node, hint))
		return -1;

	/* Shifting items from src_node to dst_node */
	if (node40_shift_items(src_node, dst_node, hint))
		return -1;

	/*
	  Checking if insert point was not moved into the corresponding
	  neighbour.
	*/
	if (hint->flags & SF_MOVIP)
		return 0;

	/* We can't splitt item at pos 0 if insert point is has 0 position too. */
	if (hint->flags & SF_LEFT && hint->pos.item == 0)
		return 0;
	
	/*
	  Estimating how many units from the border items may be shifted into
	  neighbour node.
	*/
	hint->flags = flags;
	
	if (node40_predict_units(src_node, dst_node, hint))
		return -1;

	/* Shifting units from src_node to dst_node */
	if (node40_shift_units(src_node, dst_node, hint))
		return -1;

	hint->bytes += merge.bytes;
	hint->items += merge.items;
	
	return 0;
}

#endif

static reiser4_plugin_t node40_plugin = {
	.node_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = NODE_REISER40_ID,
			.group = 0,
			.type = NODE_PLUGIN_TYPE,
			.label = "node40",
			.desc = "Node for reiserfs 4.0, ver. " VERSION,
		},
		.open		= node40_open,
		.close		= node40_close,
	
		.confirm	= node40_confirm,
		.valid		= node40_valid,
	
		.lookup		= node40_lookup,
		.items		= node40_items,
	
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

