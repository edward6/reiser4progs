/*
  node.c -- reiser4 formated node code.
  Copyright (C) 1996-2002 Hans Reiser.
*/  

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/reiser4.h>

#ifndef ENABLE_COMPACT

/* Creates node on specified device and block and with spcified key plugin */
reiser4_node_t *reiser4_node_create(
	aal_device_t *device,	/* device new node will be created on*/
	blk_t blk,		/* block new node will be created on */
	rpid_t pid,		/* node plugin id to be used */
	uint8_t level)		/* node level */
{
	reiser4_node_t *node;
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-121", device != NULL, return NULL);

	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
    
	if (!(node->block = aal_block_create(device, blk, 0)))
		goto error_free_node;
    
	/* Finding the node plugin by its id */
	if (!(plugin = libreiser4_factory_ifind(NODE_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find node plugin by its id 0x%x.", pid);
		goto error_free_block;
	}
    
	/* Requesting the plugin for initialization of the entity */
	if (!(node->entity = plugin_call(goto error_free_node, 
					 plugin->node_ops, create, node->block, level))) 
	{
		aal_exception_error("Can't create node entity.");
		goto error_free_block;
	}
    
	return node;
    
 error_free_node:    
	aal_free(node);

 error_free_block:
	aal_block_close(node->block);

	return NULL;
}

/* Saves specified node to its device */
errno_t reiser4_node_sync(
	reiser4_node_t *node)	/* node to be save */
{
	aal_assert("umka-798", node != NULL, return -1);
	return aal_block_sync(node->block);
}

#endif

/* This function is trying to detect node plugin */
static reiser4_plugin_t *reiser4_node_guess(
	aal_block_t *block)	/* block node lies in */
{
	rpid_t pid;
    
	aal_assert("umka-902", block != NULL, return NULL);
    
	pid = *((uint16_t *)block->data);
    
	/* Finding node plugin by its id from node header */
	return libreiser4_factory_ifind(NODE_PLUGIN_TYPE, pid);
}

/* Opens node on specified device and block number */
reiser4_node_t *reiser4_node_open(
	aal_device_t *device,	/* device node will be opened on */
	blk_t blk)		/* block number node will be opened on */
{
	reiser4_node_t *node;
	reiser4_plugin_t *plugin;

	aal_assert("umka-160", device != NULL, return NULL);
   
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;
   
	if (!(node->block = aal_block_open(device, blk))) {
		aal_exception_error("Can't read block %llu. %s.",
				    blk, device->error);
		goto error_free_node;
	}
    
	/* Finding the node plugin by its id */
	if (!(plugin = reiser4_node_guess(node->block))) {
		aal_exception_error("Can't guess node plugin for node %llu.", 
				    aal_block_number(node->block));
		goto error_free_block;
	}
    
	/* 
	   Initializing node's entity by means of calling "open" method of node
	   plugin.
	*/
	if (!(node->entity = plugin_call(goto error_free_node, 
					 plugin->node_ops, open, node->block)))
	{
		aal_exception_error("Can't initialize node entity.");
		goto error_free_block;
	}
	    
	return node;
    
 error_free_node:
	aal_free(node);

 error_free_block:
	aal_block_close(node->block);
    
	return NULL;
}

/* Closes specified node */
errno_t reiser4_node_close(reiser4_node_t *node) {
	aal_assert("umka-824", node != NULL, return -1);
	aal_assert("umka-903", node->entity != NULL, return -1);
    
	plugin_call(return -1, node->entity->plugin->node_ops,
		    close, node->entity);
	    
	aal_block_close(node->block);
	aal_free(node);

	return 0;
}

/* Gets left delemiting key from the specified node */
errno_t reiser4_node_lkey(
	reiser4_node_t *node,	/* node the ldkey will be obtained from */
	reiser4_key_t *key)	/* key pointer found key will be stored in */
{
	reiser4_coord_t coord;
	reiser4_pos_t pos = {0, ~0ul};

	aal_assert("umka-753", node != NULL, return -1);
	aal_assert("umka-754", key != NULL, return -1);

	if (reiser4_coord_open(&coord, node, CT_NODE, &pos))
		return -1;

	return reiser4_item_key(&coord, key);
}


#ifndef ENABLE_COMPACT

/* 
   Wrapper for reiser4_node_relocate function. This function actually copies
   item specified by src* params to dst* location. Parametrs meaning the same as
   in reiser4_node_relocate.
*/
errno_t reiser4_node_copy(reiser4_node_t *dst_node, reiser4_pos_t *dst_pos,
			  reiser4_node_t *src_node, reiser4_pos_t *src_pos) 
{
	reiser4_coord_t coord;
	reiser4_item_hint_t hint;

	aal_assert("umka-799", src_node != NULL, return -1);
	aal_assert("umka-800", dst_node != NULL, return -1);

	if (reiser4_coord_open(&coord, src_node, CT_NODE, src_pos))
		return -1;
	
	hint.len = reiser4_item_len(&coord);
	hint.data = reiser4_item_body(&coord);
	hint.plugin = reiser4_item_plugin(&coord);
    
	/* Getting the key of item that is going to be copied */
	reiser4_item_key(&coord, (reiser4_key_t *)&hint.key);

	/* Insering the item into new location */
	if (reiser4_node_insert(dst_node, dst_pos, &hint))
		return -1;
    
	return 0;
}

/* 
   Wrapper for reiser4_node_relocate function. This function actually moves item
   specified by src* params to dst* location. Parameters meaning the same as in
   previous one case.
*/
errno_t reiser4_node_move(reiser4_node_t *dst_node, 
			  reiser4_pos_t *dst_pos, reiser4_node_t *src_node, 
			  reiser4_pos_t *src_pos) 
{
	if (reiser4_node_copy(dst_node, dst_pos, src_node, src_pos))
		return -1;
    
	return reiser4_node_remove(src_node, src_pos);
}

/* 
   Splits node by means of moving right half of node into specified "right"
   node.
*/
errno_t reiser4_node_split(
	reiser4_node_t *node,	/* node to be splitted */
	reiser4_node_t *right)	/* node right half of splitted node will be stored */
{
	uint32_t median;
	reiser4_pos_t src_pos;
	reiser4_pos_t dst_pos = {0, ~0ul};
    
	aal_assert("umka-780", node != NULL, return -1);
	aal_assert("umka-781", right != NULL, return -1);

	median = reiser4_node_count(node) / 2;
	while (reiser4_node_count(node) > median) {
		src_pos.unit = ~0ul;
		src_pos.item = reiser4_node_count(node) - 1;
	
		if (reiser4_node_move(right, &dst_pos, node, &src_pos))
			return -1;
	}
    
	return 0;
}

/* Checks node for validness */
errno_t reiser4_node_valid(
	reiser4_node_t *node)	/* node to be checked */
{
	aal_assert("umka-123", node != NULL, return -1);
    
	return plugin_call(return -1, node->entity->plugin->node_ops, 
			   valid, node->entity);
}

#endif

int reiser4_node_confirm(reiser4_node_t *node) {
	aal_assert("umka-123", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   confirm, node->block);
}

/* 
   This function makes lookup inside specified node in order to find item/unit
   stored in it.
*/
int reiser4_node_lookup(
	reiser4_node_t *node,	/* node to be grepped */
	reiser4_key_t *key,	/* key to be find */
	reiser4_pos_t *pos)	/* found pos will be stored here */
{
	int lookup;

	item_entity_t *item;
	reiser4_key_t maxkey;
	reiser4_coord_t coord;
    
	aal_assert("umka-475", pos != NULL, return -1);
	aal_assert("vpf-048", node != NULL, return -1);
	aal_assert("umka-476", key != NULL, return -1);

	pos->item = 0;
	pos->unit = ~0ul;

	if (reiser4_node_count(node) == 0)
		return 0;
   
	/* Calling node plugin */
	if ((lookup = plugin_call(return -1, node->entity->plugin->node_ops,
				  lookup, node->entity, key, pos)) == -1) 
	{
		aal_exception_error("Lookup in the node %llu failed.", 
				    aal_block_number(node->block));
		return -1;
	}

	if (lookup == 1) return 1;

	if (reiser4_coord_open(&coord, node, CT_NODE, pos)) {
		aal_exception_error("Can't open item by coord. Node %llu, item %u.",
				    aal_block_number(node->block), pos->item);
		return -1;
	}

	item = &coord.entity;
		
	/*
	  We are on the position where key is less then wanted. Key could lies 
	  within the item or after the item.
	*/
	if (item->plugin->item_ops.max_poss_key) {
		
		/* FIXME-UMKA: Here should not be hardcoded key40 plugin id */
		maxkey.plugin = libreiser4_factory_ifind(KEY_PLUGIN_TYPE, 
							 KEY_REISER40_ID);

		if (item->plugin->item_ops.max_poss_key(item, &maxkey) == -1) {
			aal_exception_error("Getting max key of the item %d "
					    "in the node %llu failed.", pos->item, 
					    aal_block_number(node->block));
			return -1;
		}
	
		if (reiser4_key_compare(key, &maxkey) > 0) {
			pos->item++;
			return 0;
		}
	}

	/* Calling lookup method of found item (most probably direntry item) */
	if (!item->plugin->item_ops.lookup)
		return 0;
	    
	if ((lookup = item->plugin->item_ops.lookup(item, key, &pos->unit)) == -1) {
		aal_exception_error("Lookup in the item %d in the node %llu failed.", 
				    pos->item, aal_block_number(node->block));
		return -1;
	}

	return lookup;
}

/* Returns real item count in specified node */
uint32_t reiser4_node_count(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   count, node->entity);
}

#ifndef ENABLE_COMPACT

/* Removes specified by pos item from node */
errno_t reiser4_node_remove(
	reiser4_node_t *node,	/* node item will be removed from */
	reiser4_pos_t *pos)	/* position item will be removed at */
{
	aal_assert("umka-767", node != NULL, return -1);
	aal_assert("umka-768", pos != NULL, return -1);

	if (pos->unit == ~0ul) {
		return plugin_call(return -1, node->entity->plugin->node_ops, 
				   remove, node->entity, pos);
	} else {
		reiser4_coord_t coord;
	
		if (reiser4_coord_open(&coord, node, CT_NODE, pos)) {
			aal_exception_error("Can't open item by coord. Node %llu, item %u.",
					    aal_block_number(node->block), pos->item);
			return -1;
		}

		if (reiser4_item_count(&coord) > 1) {
			return plugin_call(return -1, node->entity->plugin->node_ops, 
					   cut, node->entity, pos);
		} else {
			return plugin_call(return -1, node->entity->plugin->node_ops, 
					   remove, node->entity, pos);
		}
	}
}

/* Inserts item described by item hint into specified node at specified pos */
errno_t reiser4_node_insert(
	reiser4_node_t *node,	    /* node new item will be inserted in */
	reiser4_pos_t *pos,	    /* position new item will be inserted at */
	reiser4_item_hint_t *hint)  /* item hint */
{
	errno_t ret;
    
	aal_assert("vpf-111", node != NULL, return -1);
	aal_assert("vpf-110", hint != NULL, return -1);
	aal_assert("vpf-108", pos != NULL, return -1);

	if (!hint->data) {
		/* 
		   Estimate the size that will be spent for item. This should be
		   done if item->data not installed.
		*/
		if (hint->len == 0) {
			reiser4_coord_t coord;
	    
			if (reiser4_coord_init(&coord, node, CT_NODE, pos))
				return -1;
	    
			if (reiser4_item_estimate(&coord, hint)) {
				aal_exception_error("Can't estimate space that "
						    "item being inserted will consume.");
				return -1;
			}
		}
	} else {
		aal_assert("umka-761", hint->len > 0 && 
			   hint->len < reiser4_node_maxspace(node), return -1);
	}
    
	/* Checking if item length is gretter then free space in node */
	if (hint->len + (pos->unit == ~0ul ? reiser4_node_overhead(node) : 0) >
	    reiser4_node_space(node))
	{
		char *target = (pos->unit == ~0ul ? "item" : "unit");
		aal_exception_error("There is no space to insert the %s of (%u) "
				    "size in the node (%llu).", target, 
				    hint->len, aal_block_number(node->block));
		return -1;
	}

	/* 
	   Inserting new item or passting unit into one existent item pointed by
	   pos->item.
	*/
	if (pos->unit == ~0ul) {
		if ((ret = plugin_call(return -1, node->entity->plugin->node_ops, 
				       insert, node->entity, pos, hint)) != 0)
			return ret;
	} else {
		if ((ret = plugin_call(return -1, node->entity->plugin->node_ops, 
				       paste, node->entity, pos, hint)) != 0)
			return ret;
	}
    
	return 0;
}

#endif

/* Returns node plugin id in use */
uint16_t reiser4_node_pid(
	reiser4_node_t *node)	/* node pid to be obtained */
{
	aal_assert("umka-828", node != NULL, return FAKE_PLUGIN);
    
	return plugin_call(return 0, node->entity->plugin->node_ops,
			   pid, node->entity);
}

/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL, return 0);

	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL, return 0);
    
	return plugin_call(return 0, node->entity->plugin->node_ops, 
			   maxspace, node->entity);
}

