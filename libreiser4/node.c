/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.c -- the reiser4 disk node personalization. The libreiser4 internal
   in-memory tree consists of node_t instances. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_node_isdirty(node_t *node) {
	uint32_t state;
	
	aal_assert("umka-2663", node != NULL);

	state = plug_call(node->entity->plug->o.node_ops,
			  get_state, node->entity);
	
	if (state & (1 << ENTITY_DIRTY))
		return 1;

	return node->entity->block->dirty;
}

void reiser4_node_mkdirty(node_t *node) {
	uint32_t state;
	
	aal_assert("umka-2662", node != NULL);

	state = plug_call(node->entity->plug->o.node_ops,
			  get_state, node->entity);

	state |= (1 << ENTITY_DIRTY);
	
	plug_call(node->entity->plug->o.node_ops,
		  set_state, node->entity, state);
	
	node->entity->block->dirty = 1;
}

void reiser4_node_mkclean(node_t *node) {
	uint32_t state;
	
	aal_assert("umka-2661", node != NULL);

	state = plug_call(node->entity->plug->o.node_ops,
			  get_state, node->entity);

	state &= ~(1 << ENTITY_DIRTY);
	
	plug_call(node->entity->plug->o.node_ops,
		  set_state, node->entity, state);

	node->entity->block->dirty = 0;
}

/* Clones node @src to @dst. */
errno_t reiser4_node_clone(node_t *src, node_t *dst) {
	aal_assert("umka-2306", src != NULL);
	aal_assert("umka-2307", dst != NULL);

	return plug_call(src->entity->plug->o.node_ops,
			 clone, src->entity, dst->entity);
}

/* Creates new node at block @nr on @tree with @level and with plugin @pid. Uses
   tree instance for accessing block size and key plugin in use. */
node_t *reiser4_node_create(reiser4_tree_t *tree, blk_t nr,
			    rid_t pid, uint8_t level)
{
	node_t *node;
	uint32_t size;
	aal_block_t *block;
	reiser4_plug_t *plug;
	aal_device_t *device;

	aal_assert("umka-1268", tree != NULL);
    
	/* Finding the node plugin by its id */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find node plugin by its "
				    "id 0x%x.", pid);
		return NULL;
	}

	/* Getting tree tree device and blksize in use to use them for creating
	   new node. */
	size = reiser4_tree_get_blksize(tree);
	device = reiser4_tree_get_device(tree);

	/* Allocate new node of @size at @nr. */
	if (!(block = aal_block_alloc(device, size, nr)))
		return NULL;

	/* Allocating memory for instance of node */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		goto error_free_block;

	/* Requesting the plugin for initialization node entity. */
	if (!(node->entity = plug_call(plug->o.node_ops, init,
				       block, tree->key.plug)))
	{
		goto error_free_node;
	}

	reiser4_place_assign(&node->p, NULL, 0, MAX_UINT32);

	/* Making node header. */
	if (reiser4_node_fresh(node, level))
		goto error_free_entity;
	
	return node;

 error_free_entity:
	plug_call(plug->o.node_ops, fini, node->entity);
 error_free_node:    
	aal_free(node);
 error_free_block:
	aal_block_free(block);
	return NULL;
}

/* Packes @node to @stream. */
errno_t reiser4_node_pack(node_t *node, aal_stream_t *stream) {
	aal_assert("umka-2622", node != NULL);
	aal_assert("umka-2623", stream != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 pack, node->entity, stream);
}

/* Create node from passed @stream. */
node_t *reiser4_node_unpack(reiser4_tree_t *tree, aal_stream_t *stream) {
	blk_t blk;
	rid_t pid;
	
	node_t *node;
	uint32_t size;
	aal_block_t *block;
	reiser4_plug_t *plug;
	aal_device_t *device;
	
	aal_assert("umka-2624", tree != NULL);
	aal_assert("umka-2625", stream != NULL);

	aal_stream_read(stream, &pid, sizeof(pid));
	aal_stream_read(stream, &blk, sizeof(blk));
	
	/* Finding the node plugin by its id */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find node plugin by its id "
				    "0x%x.", pid);
		return NULL;
	}

	size = reiser4_tree_get_blksize(tree);
	device = reiser4_tree_get_device(tree);

	/* Allocate new node of @size at @nr. */
	if (!(block = aal_block_alloc(device, size, blk)))
		return NULL;

	/* Allocating memory for instance of node */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		goto error_free_block;

	/* Requesting the plugin for initialization node entity. */
	if (!(node->entity = plug_call(plug->o.node_ops, unpack,
				       block, tree->key.plug, stream)))
	{
		goto error_free_node;
	}

	return node;
 error_free_node:    
	aal_free(node);
 error_free_block:
	aal_block_free(block);
	return NULL;
}
#endif

/* Functions for lock/unlock @node. They are used to prevent releasing node from
   the tree cache. */
void reiser4_node_lock(node_t *node) {
	aal_assert("umka-2314", node != NULL);
	aal_assert("umka-2585", node->counter >= 0);
	node->counter++;
}

void reiser4_node_unlock(node_t *node) {
	aal_assert("umka-2316", node != NULL);
	aal_assert("umka-2316", node->counter > 0);
	node->counter--;
}

bool_t reiser4_node_locked(node_t *node) {
	aal_assert("umka-2586", node != NULL);
	aal_assert("umka-2587", node->counter >= 0);
	return node->counter > 0 ? TRUE : FALSE;
}

#ifndef ENABLE_STAND_ALONE
/* Assigns @nr block number to @node. */
void reiser4_node_move(node_t *node, blk_t nr) {
	aal_assert("umka-2248", node != NULL);
	
	plug_call(node->entity->plug->o.node_ops,
		  move, node->entity, nr);
}

/* Call node plugin fresh() method, which creates new node header with zero
   items in it, etc. */
errno_t reiser4_node_fresh(node_t *node, uint8_t level) {
	aal_assert("umka-2052", node != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 fresh, node->entity, level);
}

/* Print passed @node to the specified @stream */
errno_t reiser4_node_print(
	node_t *node,           /* node to be printed */
	aal_stream_t *stream)   /* stream for printing in */
{
	aal_assert("umka-1537", node != NULL);
	aal_assert("umka-1538", stream != NULL);
	
	return plug_call(node->entity->plug->o.node_ops,
			 print, node->entity, stream,
			 -1, -1, 0);
}
#endif

/* Opens node on specified @tree and block number @nr. */
node_t *reiser4_node_open(reiser4_tree_t *tree, blk_t nr) {
	uint16_t pid;
        node_t *node;
	
	uint32_t size;
	aal_block_t *block;
	aal_device_t *device;
	reiser4_plug_t *plug;
 
        aal_assert("umka-160", tree != NULL);

	/* Getting tree characteristics needed for open node. */
	size = reiser4_tree_get_blksize(tree);
	device = reiser4_tree_get_device(tree);
	
        if (!(node = aal_calloc(sizeof(*node), 0)))
                return NULL;

	/* Load block at @nr, that node lie in. */
	if (!(block = aal_block_load(device, size, nr))) {
		aal_exception_error("Can't load node %llu. %s.",
				    nr, device->error);
		goto error_free_node;
	}

	/* Getting node plugin id. */
	pid = *((uint16_t *)block->data);

	/* Finding the node plug by its id */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid)))
		goto error_free_block;

	/* Requesting the plugin for initialization of the entity */
	if (!(node->entity = plug_call(plug->o.node_ops, init,
				       block, tree->key.plug)))
	{
		goto error_free_block;
	}
	
        reiser4_place_assign(&node->p, NULL, 0, MAX_UINT32);
	return node;
	
 error_free_block:
	aal_block_free(block);
 error_free_node:
        aal_free(node);
        return NULL;
}

/* Saves node to device if it is dirty and closes node */
errno_t reiser4_node_fini(node_t *node) {
#ifndef ENABLE_STAND_ALONE
	/* Node should be clean when it is going to be closed. */
	if (reiser4_node_isdirty(node) && reiser4_node_sync(node)) {
		aal_exception_error("Can't write node %llu.",
				    node_blocknr(node));
	}
#endif

	return reiser4_node_close(node);
}

/* Closes specified node and its children. Before the closing, this function
   also detaches nodes from the tree if they were attached. */
errno_t reiser4_node_close(node_t *node) {
	aal_assert("umka-824", node != NULL);
	aal_assert("umka-2286", node->counter == 0);

	plug_call(node->entity->plug->o.node_ops,
		  fini, node->entity);
	    
	aal_free(node);
	return 0;
}

/* Getting the left delimiting key. */
errno_t reiser4_node_leftmost_key(
	node_t *node,	            /* node for working with */
	reiser4_key_t *key)	    /* key will be stored here */
{
	pos_t pos = {0, MAX_UINT32};

	aal_assert("umka-754", key != NULL);
	aal_assert("umka-753", node != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 get_key, node->entity, &pos, key);
}

#ifndef ENABLE_STAND_ALONE
/* Acknowledles, that passed @place has nodeptr that points onto passed
   @node. This is needed for node_realize() function. */
static int reiser4_node_ack(node_t *node, place_t *place) {
	blk_t blk;
	
	if (!(place->pos.item < reiser4_node_items(place->node)))
		return 0;
	       
	if (reiser4_place_fetch(place))
		return 0;

	if (!reiser4_item_branch(place->plug))
		return 0;

	blk = reiser4_item_down_link(place);
	return blk == node_blocknr(node);
}
#endif

/* Updates @node->p by position in parent node. */
errno_t reiser4_node_realize(
	node_t *node)             /* node position will be obtained for */
{
	place_t *parent;
        reiser4_key_t lkey;
    
	aal_assert("umka-869", node != NULL);
	aal_assert("umka-1941", node->p.node != NULL);

	parent = &node->p;

	/* Checking if we are in position already */
#ifndef ENABLE_STAND_ALONE
	if (!(node->flags & NF_FOREIGN)) {
		if (reiser4_node_ack(node, parent))
			goto parent_fetch;
	}
#endif

        reiser4_node_leftmost_key(node, &lkey);
                                                                                                   
	/* Getting position by means of using node lookup. */
        if (reiser4_node_lookup(parent->node, &lkey, FIND_EXACT,
				&parent->pos) == PRESENT)
	{
#ifndef ENABLE_STAND_ALONE
		if (!(node->flags & NF_FOREIGN)) {
			if (reiser4_node_ack(node, parent))
				goto parent_fetch;
		} else
#endif
			goto parent_fetch;
	}

	/* Getting position by means of linear traverse */
#ifndef ENABLE_STAND_ALONE
	if (!(node->flags & NF_FOREIGN)) {
		blk_t blk;
		lookup_t res;
		uint32_t i, j;
		
		for (i = 0; i < reiser4_node_items(parent->node); i++) {
			parent->pos.item = i;

			if ((res = reiser4_place_fetch(parent)))
				return res;

			if (!reiser4_item_branch(parent->plug))
				continue;

			for (j = 0; j < reiser4_item_units(parent); j++) {
				parent->pos.unit = j;
				
				blk = reiser4_item_down_link(parent);
				
				if (blk == node_blocknr(node))
					goto parent_fetch;
			}
		}
	}

	return -EINVAL;
#endif
	
 parent_fetch:
	if (reiser4_place_fetch(parent))
		return -EINVAL;

	if (reiser4_item_units(parent) == 1)
		parent->pos.unit = MAX_UINT32;

	return 0;
}

/* This function makes search inside of specified node for passed key. Position
   will be stored in passed @pos. */
lookup_t reiser4_node_lookup(node_t *node, reiser4_key_t *key,
			     bias_t bias, pos_t *pos)
{
	lookup_t res;
	reiser4_key_t maxkey;
	place_t place;
    
	aal_assert("umka-475", pos != NULL);
	aal_assert("vpf-048", node != NULL);
	aal_assert("umka-476", key != NULL);

	POS_INIT(pos, 0, MAX_UINT32);

	/* Calling node plugin lookup method */
	if ((res = plug_call(node->entity->plug->o.node_ops,
			     lookup, node->entity, key, bias,
			     pos)) < 0)
	{
		return res;
	}

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

			if (reiser4_key_compfull(key, &maxkey) > 0) {
				pos->item++;
				return ABSENT;
			}
		}
	
		/* Calling lookup method of found item */
		if (place.plug->o.item_ops->balance->lookup) {
			res = plug_call(place.plug->o.item_ops->balance,
					lookup, &place, key, bias);
			
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

	return PRESENT;
}

/* Returns real item count in specified node */
uint32_t reiser4_node_items(node_t *node) {
	aal_assert("umka-453", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 items, node->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns free space of specified node */
uint16_t reiser4_node_space(node_t *node) {
	aal_assert("umka-455", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(node_t *node) {
	aal_assert("vpf-066", node != NULL);

	return plug_call(node->entity->plug->o.node_ops, 
			 overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(node_t *node) {
	aal_assert("umka-125", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 maxspace, node->entity);
}

/* Expands passed @node at @pos by @len */
errno_t reiser4_node_expand(node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1815", node != NULL);
	aal_assert("umka-1816", pos != NULL);

	res = plug_call(node->entity->plug->o.node_ops,
			expand, node->entity, pos, len, count);

	return res;
}

/* Shrinks passed @node at @pos by @len */
errno_t reiser4_node_shrink(node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1817", node != NULL);
	aal_assert("umka-1818", pos != NULL);

	if ((res = plug_call(node->entity->plug->o.node_ops,
			     shrink, node->entity, pos, len, count)))
	{
		aal_exception_error("Node %llu, pos %u/%u: can't "
				    "shrink the node on %u bytes.", 
				    node_blocknr(node), pos->item, 
				    pos->unit, len);
	}

	return res;
}

/* Makes shift of some amount of items and units into passed neighbour. Shift
   direction and other flags are passed by @hint. Returns operation error
   code. */
errno_t reiser4_node_shift(node_t *node, node_t *neig,
			   shift_hint_t *hint)
{
	aal_assert("umka-1225", node != NULL);
	aal_assert("umka-1226", neig != NULL);
	aal_assert("umka-1227", hint != NULL);

	/* Trying shift something from @node into @neig. As result insert point
	   may be shifted too. */
	return plug_call(node->entity->plug->o.node_ops, shift,
			 node->entity, neig->entity, hint);
}

/* Saves passed @node onto device it was opened on */
errno_t reiser4_node_sync(node_t *node) {
	aal_assert("umka-2253", node != NULL);
    
	/* Synchronizing passed @node */
	if (!reiser4_node_isdirty(node)) 
		return 0;
	
	return plug_call(node->entity->plug->o.node_ops, 
			 sync, node->entity);
}

/* Updates nodeptr item in parent node */
errno_t reiser4_node_update_ptr(node_t *node) {
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
errno_t reiser4_node_update_key(node_t *node, pos_t *pos,
				reiser4_key_t *key)
{
	aal_assert("umka-999", node != NULL);
	aal_assert("umka-1000", pos != NULL);
	aal_assert("umka-1001", key != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 set_key, node->entity, pos, key);
}

/* Node modifying fucntion. */
int64_t reiser4_node_modify(node_t *node, pos_t *pos,
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
	
	/* Checking if item length is greater then free space in the node */
	if (needed > reiser4_node_space(node)) {
		aal_exception_error("There is no space to insert new "
				    "item/unit of (%u) size in the node "
				    "(%llu).", len, node_blocknr(node));
		return -EINVAL;
	}

	/* Modifing the node with the given @hint. */
	if ((write = modify_func(node, pos, hint)) < 0)
		return write;

	return write;
}

errno_t callback_node_insert(node_t *node, pos_t *pos,
			     trans_hint_t *hint) 
{
	return plug_call(node->entity->plug->o.node_ops,
			 insert, node->entity, pos, hint);
}

errno_t callback_node_write(node_t *node, pos_t *pos,
			    trans_hint_t *hint) 
{
	return plug_call(node->entity->plug->o.node_ops,
			 write, node->entity, pos, hint);
}

errno_t reiser4_node_insert(node_t *node, pos_t *pos,
			    trans_hint_t *hint)
{
	aal_assert("umka-990", node != NULL);
	aal_assert("umka-991", pos != NULL);
	aal_assert("umka-992", hint != NULL);
	
	return reiser4_node_modify(node, pos, hint,
				   callback_node_insert);
}

int64_t reiser4_node_write(node_t *node,
			   pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2445", node != NULL);
	aal_assert("umka-2446", pos != NULL);
	aal_assert("umka-2447", hint != NULL);

	return reiser4_node_modify(node, pos, hint,
				   callback_node_write);
}

int64_t reiser4_node_trunc(node_t *node, pos_t *pos,
			   trans_hint_t *hint)
{
	aal_assert("umka-2503", node != NULL);
	aal_assert("umka-2504", pos != NULL);
	aal_assert("umka-2505", hint != NULL);
	
	return plug_call(node->entity->plug->o.node_ops,
			 trunc, node->entity, pos, hint);
}

/* Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key. */
errno_t reiser4_node_remove(node_t *node, pos_t *pos,
			    trans_hint_t *hint)
{
	errno_t res;
	
	aal_assert("umka-993", node != NULL);
	aal_assert("umka-994", pos != NULL);
	aal_assert("umka-2391", hint != NULL);

	/* Removing item or unit. We assume that we remove whole item if unit
	   component is set to MAX_UINT32. Otherwise we remove unit. */
	if ((res = plug_call(node->entity->plug->o.node_ops,
			     remove, node->entity, pos, hint)))
	{
		aal_exception_error("Can't remove %llu items/units "
				    "from node %llu.", hint->count,
				    node_blocknr(node));
		return res;
	}

	return 0;
}

void reiser4_node_set_mstamp(node_t *node, uint32_t stamp) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-646", node != NULL);
	aal_assert("umka-1969", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->set_mstamp)
		plug->o.node_ops->set_mstamp(node->entity, stamp);
}

void reiser4_node_set_fstamp(node_t *node, uint64_t stamp) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-648", node != NULL);
	aal_assert("umka-1970", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->get_fstamp)
		plug->o.node_ops->set_fstamp(node->entity, stamp);
}

void reiser4_node_set_level(node_t *node, uint8_t level) {
	aal_assert("umka-1863", node != NULL);
    
	plug_call(node->entity->plug->o.node_ops, 
		  set_level, node->entity, level);
}

uint32_t reiser4_node_get_mstamp(node_t *node) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-562", node != NULL);
	aal_assert("umka-1971", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->get_mstamp)
		return plug->o.node_ops->get_mstamp(node->entity);
	
	return 0;
}

uint64_t reiser4_node_get_fstamp(node_t *node) {
	reiser4_plug_t *plug;

	aal_assert("vpf-647", node != NULL);
	aal_assert("umka-1972", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->get_fstamp)
		plug->o.node_ops->get_fstamp(node->entity);

	return 0;
}
#endif

/* Returns node level */
uint8_t reiser4_node_get_level(node_t *node) {
	aal_assert("umka-1642", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 get_level, node->entity);
}
