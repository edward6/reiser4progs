/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.c -- the reiser4 disk node personalization. The libreiser4 internal
   in-memory tree consists of reiser4_node_t instances. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_node_isdirty(reiser4_node_t *node) {
	aal_assert("umka-2663", node != NULL);
	return node->block->dirty;
}

void reiser4_node_mkdirty(reiser4_node_t *node) {
	aal_assert("umka-2662", node != NULL);
	node->block->dirty = 1;
}

void reiser4_node_mkclean(reiser4_node_t *node) {
	aal_assert("umka-2661", node != NULL);
	node->block->dirty = 0;
}

/* Creates new node at block @nr on @tree with @level and with plugin @pid. Uses
   tree instance for accessing block size and key plugin in use. */
reiser4_node_t *reiser4_node_create(reiser4_tree_t *tree, blk_t nr,
				    rid_t pid, uint8_t level)
{
	uint32_t size;
	aal_block_t *block;
	reiser4_node_t *node;
	reiser4_plug_t *plug;
	aal_device_t *device;

	aal_assert("umka-1268", tree != NULL);
    
	/* Finding the node plugin by its id */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid))) {
		aal_error("Can't find node plugin by its id 0x%x.",
			  pid);
		return NULL;
	}

	/* Getting tree tree device and blksize in use to use them for creating
	   new node. */
	size = reiser4_tree_get_blksize(tree);
	device = reiser4_tree_get_device(tree);

	/* Allocate new node of @size at @nr. */
	if (!(block = aal_block_alloc(device, size, nr)))
		return NULL;

	/* Requesting the plugin for initialization node entity. */
	if (!(node = plug_call(plug->o.node_ops, init,
			       block, level, tree->key.plug)))
	{
		goto error_free_block;
	}

	reiser4_place_assign(&node->p, NULL, 0, MAX_UINT32);

	return node;

 error_free_block:
	aal_block_free(block);
	return NULL;
}

/* Traverse through all items of the gived node. */
errno_t reiser4_node_trav(reiser4_node_t *node, place_func_t func, void *data) {
	reiser4_place_t place;
	pos_t *pos = &place.pos;
	errno_t res;
	
	aal_assert("vpf-744", node != NULL);
	
	pos->unit = MAX_UINT32;
	
	for (pos->item = 0; pos->item < reiser4_node_items(node); pos->item++) {
		if ((res = reiser4_place_open(&place, node, pos))) {
			aal_error("Node (%llu), item (%u): failed to "
				  "open the item by its place.", 
				  node_blocknr(node), pos->item);
			return res;
		}
		
		if ((res = func(&place, data)))
			return res;
	}
	
	return 0;
}

#endif

/* Functions for lock/unlock @node. They are used to prevent releasing node from
   the tree cache. */
void reiser4_node_lock(reiser4_node_t *node) {
	aal_assert("umka-2314", node != NULL);
	aal_assert("umka-2585", node->counter >= 0);
	node->counter++;
}

void reiser4_node_unlock(reiser4_node_t *node) {
	aal_assert("umka-2316", node != NULL);
	aal_assert("umka-2316", node->counter > 0);
	node->counter--;
}

bool_t reiser4_node_locked(reiser4_node_t *node) {
	aal_assert("umka-2586", node != NULL);
	aal_assert("umka-2587", node->counter >= 0);
	return node->counter > 0 ? 1 : 0;
}

#ifndef ENABLE_STAND_ALONE
/* Assigns @nr block number to @node. */
void reiser4_node_move(reiser4_node_t *node, blk_t nr) {
	aal_assert("umka-2248", node != NULL);

	node->block->nr = nr;
	reiser4_node_mkdirty(node);
}
#endif

/* Opens node on specified @tree and block number @nr. */
reiser4_node_t *reiser4_node_open(reiser4_tree_t *tree, blk_t nr) {
	uint16_t pid;
	uint32_t size;

	aal_block_t *block;
	aal_device_t *device;
	reiser4_plug_t *plug;
        reiser4_node_t *node;
 
        aal_assert("umka-160", tree != NULL);

	/* Getting tree characteristics needed for open node. */
	size = reiser4_tree_get_blksize(tree);
	device = reiser4_tree_get_device(tree);
	
	/* Load block at @nr, that node lie in. */
	if (!(block = aal_block_load(device, size, nr))) {
		aal_error("Can't load node %llu. %s.",
			  nr, device->error);
		return NULL;
	}

	/* Getting node plugin id. */
	pid = *((uint16_t *)block->data);

	/* Finding the node plug by its id. */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid)))
		goto error_free_block;

	/* Requesting the plugin for initialization of the entity. */
	if (!(node = plug_call(plug->o.node_ops, open,
			       block, tree->key.plug)))
	{
		goto error_free_block;
	}
	
        reiser4_place_assign(&node->p, NULL, 0, MAX_UINT32);
	
	return node;
	
 error_free_block:
	aal_block_free(block);
        return NULL;
}

/* Saves node to device if it is dirty and closes node */
errno_t reiser4_node_fini(reiser4_node_t *node) {
#ifndef ENABLE_STAND_ALONE
	/* Node should be clean when it is going to be closed. */
	if (reiser4_node_isdirty(node) && reiser4_node_sync(node)) {
		aal_error("Can't write node %llu.", node_blocknr(node));
	}
#endif

	return reiser4_node_close(node);
}

/* Closes specified node and its children. Before the closing, this function
   also detaches nodes from the tree if they were attached. */
errno_t reiser4_node_close(reiser4_node_t *node) {
	aal_assert("umka-824", node != NULL);
	aal_assert("umka-2286", node->counter == 0);

	plug_call(node->plug->o.node_ops, fini, node);
	return 0;
}

/* Getting the left delimiting key. */
errno_t reiser4_node_leftmost_key(
	reiser4_node_t *node,	            /* node for working with */
	reiser4_key_t *key)	    /* key will be stored here */
{
	pos_t pos = {0, MAX_UINT32};

	aal_assert("umka-754", key != NULL);
	aal_assert("umka-753", node != NULL);

	return plug_call(node->plug->o.node_ops,
			 get_key, node, &pos, key);
}

/* This function makes search inside of specified node for passed key. Position
   will be stored in passed @pos. */
lookup_t reiser4_node_lookup(reiser4_node_t *node,
			     lookup_hint_t *hint,
			     lookup_bias_t bias,
			     pos_t *pos)
{
	lookup_t res;
	reiser4_key_t maxkey;
	reiser4_place_t place;
    
	aal_assert("umka-475", pos != NULL);
	aal_assert("vpf-048", node != NULL);
	
	aal_assert("umka-476", hint != NULL);
	aal_assert("umka-3090", hint->key != NULL);

	POS_INIT(pos, 0, MAX_UINT32);

	/* Calling node plugin lookup method */
	if ((res = plug_call(node->plug->o.node_ops, lookup,
			     node, hint, bias, pos)) < 0)
	{
		return res;
	}

	/* Wanted key is not key of item. Will look inside found item in order
	   to find needed unit inside. */
	if (res == ABSENT) {
		if (pos->item == 0)
			return ABSENT;
		
		/* Correcting position. */
		pos->item--;
		
		if (reiser4_place_open(&place, node, pos))
			return -EIO;
		
		/* We are on the position where key is less then wanted. Key
		   could lie within the item or after the item. */
		if (place.plug->o.item_ops->balance->maxposs_key) {
			reiser4_item_maxposs_key(&place, &maxkey);

			if (reiser4_key_compfull(hint->key, &maxkey) > 0) {
				pos->item++;
				return ABSENT;
			}
		}
	
		/* Calling lookup method of found item. */
		if (place.plug->o.item_ops->balance->lookup) {
			res = plug_call(place.plug->o.item_ops->balance,
					lookup, &place, hint, bias);
			
			pos->unit = place.pos.unit;
			return res;
		}

		/* Check for @bias. If it is FIND_CONV, this means, that we're
		   looking for convenient pos for insert into. */
		if (bias == FIND_CONV) {
			pos->item++;
			return ABSENT;
		}
	}

	return res;
}

/* Returns real item count in specified node */
uint32_t reiser4_node_items(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL);
    
	return plug_call(node->plug->o.node_ops, 
			 items, node);
}

#ifndef ENABLE_STAND_ALONE
/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL);
    
	return plug_call(node->plug->o.node_ops, 
			 space, node);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL);

	return plug_call(node->plug->o.node_ops, 
			 overhead, node);
}

/* Returns max space in specified node. */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL);
    
	return plug_call(node->plug->o.node_ops, 
			 maxspace, node);
}

/* Expands passed @node at @pos by @len */
errno_t reiser4_node_expand(reiser4_node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	aal_assert("umka-1815", node != NULL);
	aal_assert("umka-1816", pos != NULL);

	return plug_call(node->plug->o.node_ops,
			 expand, node, pos, len, count);
}

/* Shrinks passed @node at @pos by @len */
errno_t reiser4_node_shrink(reiser4_node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1817", node != NULL);
	aal_assert("umka-1818", pos != NULL);

	if ((res = plug_call(node->plug->o.node_ops, shrink,
			     node, pos, len, count)))
	{
		aal_error("Node %llu, pos %u/%u: can't "
			  "shrink the node on %u bytes.", 
			  node_blocknr(node), pos->item, 
			  pos->unit, len);
	}

	return res;
}

/* Makes shift of some amount of items and units into passed neighbour. Shift
   direction and other flags are passed by @hint. Returns operation error
   code. */
errno_t reiser4_node_shift(reiser4_node_t *node, reiser4_node_t *neig,
			   shift_hint_t *hint)
{
	aal_assert("umka-1225", node != NULL);
	aal_assert("umka-1226", neig != NULL);
	aal_assert("umka-1227", hint != NULL);

	/* Trying shift something from @node into @neig. As result insert point
	   may be shifted too. */
	return plug_call(node->plug->o.node_ops, shift,
			 node, neig, hint);
}

errno_t reiser4_node_fuse(reiser4_node_t *node, pos_t *pos1, pos_t *pos2) {
	aal_assert("vpf-1507", node != NULL);

	return plug_call(node->plug->o.node_ops, 
			 fuse, node, pos1, pos2);
}

/* Saves passed @node onto device it was opened on */
errno_t reiser4_node_sync(reiser4_node_t *node) {
	aal_assert("umka-2253", node != NULL);
    
	/* Synchronizing passed @node */
	if (!reiser4_node_isdirty(node)) 
		return 0;
	
	return plug_call(node->plug->o.node_ops, 
			 sync, node);
}

/* Updates nodeptr item in parent node */
errno_t reiser4_node_update_ptr(reiser4_node_t *node) {
	blk_t blk;
	errno_t res;

	aal_assert("umka-2263", node != NULL);

	if (!node->p.node)
		return 0;
	
	blk = node_blocknr(node);
	
	if ((res = reiser4_place_fetch(&node->p)))
		return res;
	
	return reiser4_item_update_link(&node->p, blk);
}

/* Updates node keys in recursive maner (needed for updating ldkeys on the all
   levels of tre tree). */
errno_t reiser4_node_update_key(reiser4_node_t *node, pos_t *pos,
				reiser4_key_t *key)
{
	aal_assert("umka-999", node != NULL);
	aal_assert("umka-1000", pos != NULL);
	aal_assert("umka-1001", key != NULL);

	return plug_call(node->plug->o.node_ops,
			 set_key, node, pos, key);
}

/* Node modifying fucntion. */
int64_t reiser4_node_modify(reiser4_node_t *node, pos_t *pos,
			    trans_hint_t *hint,
			    modify_func_t modify_func)
{
	uint32_t len;
	int64_t write;
	uint32_t needed;

	aal_assert("umka-2679", pos != NULL);
	aal_assert("umka-2678", node != NULL);
	aal_assert("umka-2680", hint != NULL);
	aal_assert("umka-2681", modify_func != NULL);
	
	len = hint->len + hint->overhead;

	needed = len + (pos->unit == MAX_UINT32 ?
			reiser4_node_overhead(node) : 0);
	
	/* Checking if item length is greater then free space in the node. */
	if (needed > reiser4_node_space(node)) {
		aal_error("There is no space to insert new "
			  "item/unit of (%u) size in the node "
			  "(%llu).", len, node_blocknr(node));
		return -EINVAL;
	}

	/* Modifing the node with the given @hint. */
	if ((write = modify_func(node, pos, hint)) < 0)
		return write;

	return write;
}

errno_t callback_node_insert(reiser4_node_t *node, pos_t *pos,
			     trans_hint_t *hint) 
{
	return plug_call(node->plug->o.node_ops,
			 insert, node, pos, hint);
}

errno_t callback_node_write(reiser4_node_t *node, pos_t *pos,
			    trans_hint_t *hint) 
{
	return plug_call(node->plug->o.node_ops,
			 write, node, pos, hint);
}

errno_t reiser4_node_insert(reiser4_node_t *node, pos_t *pos,
			    trans_hint_t *hint)
{
	aal_assert("umka-991", pos != NULL);
	aal_assert("umka-990", node != NULL);
	aal_assert("umka-992", hint != NULL);
	
	return reiser4_node_modify(node, pos, hint,
				   callback_node_insert);
}

int64_t reiser4_node_write(reiser4_node_t *node, pos_t *pos,
			   trans_hint_t *hint)
{
	aal_assert("umka-2446", pos != NULL);
	aal_assert("umka-2447", hint != NULL);
	aal_assert("umka-2445", node != NULL);

	return reiser4_node_modify(node, pos, hint,
				   callback_node_write);
}

int64_t reiser4_node_trunc(reiser4_node_t *node, pos_t *pos,
			   trans_hint_t *hint)
{
	aal_assert("umka-2503", node != NULL);
	aal_assert("umka-2504", pos != NULL);
	aal_assert("umka-2505", hint != NULL);
	
	return plug_call(node->plug->o.node_ops,
			 trunc, node, pos, hint);
}

/* Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key. */
errno_t reiser4_node_remove(reiser4_node_t *node, pos_t *pos,
			    trans_hint_t *hint)
{
	errno_t res;
	
	aal_assert("umka-993", node != NULL);
	aal_assert("umka-994", pos != NULL);
	aal_assert("umka-2391", hint != NULL);

	/* Removing item or unit. We assume that we remove whole item if unit
	   component is set to MAX_UINT32. Otherwise we remove unit. */
	if ((res = plug_call(node->plug->o.node_ops,
			     remove, node, pos, hint)))
	{
		aal_error("Can't remove %llu %s from the node %llu.", 
			  hint->count, pos->unit == MAX_UINT32 ? 
			  "items" : "units", node_blocknr(node));
		return res;
	}

	return 0;
}

void reiser4_node_set_mstamp(reiser4_node_t *node, uint32_t stamp) {
	aal_assert("vpf-646", node != NULL);

	if (node->plug->o.node_ops->set_mstamp)
		node->plug->o.node_ops->set_mstamp(node, stamp);
}

void reiser4_node_set_fstamp(reiser4_node_t *node, uint64_t stamp) {
	aal_assert("vpf-648", node != NULL);
	
	if (node->plug->o.node_ops->get_fstamp)
		node->plug->o.node_ops->set_fstamp(node, stamp);
}

void reiser4_node_set_level(reiser4_node_t *node, uint8_t level) {
	aal_assert("umka-1863", node != NULL);
	plug_call(node->plug->o.node_ops, set_level, node, level);
}

uint32_t reiser4_node_get_mstamp(reiser4_node_t *node) {
	aal_assert("vpf-562", node != NULL);
	
	if (node->plug->o.node_ops->get_mstamp)
		return node->plug->o.node_ops->get_mstamp(node);
	
	return 0;
}

uint64_t reiser4_node_get_fstamp(reiser4_node_t *node) {
	aal_assert("vpf-647", node != NULL);

	if (node->plug->o.node_ops->get_fstamp)
		node->plug->o.node_ops->get_fstamp(node);

	return 0;
}
#endif

/* Returns node level */
uint8_t reiser4_node_get_level(reiser4_node_t *node) {
	aal_assert("umka-1642", node != NULL);
    
	return plug_call(node->plug->o.node_ops, 
			 get_level, node);
}
