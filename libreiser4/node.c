/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.c -- the reiser4 disk node personalization. The libreiser4 internal
   in-memory tree consists of reiser4_node_t instances. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
/* Functions for dirtying node. */
bool_t reiser4_node_isdirty(reiser4_node_t *node) {
	aal_assert("umka-2094", node != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 isdirty, node->entity);
}

void reiser4_node_mkdirty(reiser4_node_t *node) {
	aal_assert("umka-2095", node != NULL);

	plug_call(node->entity->plug->o.node_ops,
		  mkdirty, node->entity);
}

void reiser4_node_mkclean(reiser4_node_t *node) {
	aal_assert("umka-2096", node != NULL);

	plug_call(node->entity->plug->o.node_ops,
		  mkclean, node->entity);
}

/* Clones node @src to @dst. */
errno_t reiser4_node_clone(reiser4_node_t *src,
			   reiser4_node_t *dst)
{
	aal_assert("umka-2306", src != NULL);
	aal_assert("umka-2307", dst != NULL);

	return plug_call(src->entity->plug->o.node_ops,
			 clone, src->entity, dst->entity);
}

/* Creates new node at block @nr on @tree with @level and with plugin @pid. Uses
   tree instance for accessing block size and key plugin in use. */
reiser4_node_t *reiser4_node_create(reiser4_tree_t *tree,
				    blk_t nr, rid_t pid,
				    uint8_t level)
{
	uint32_t size;
	aal_block_t *block;
	reiser4_node_t *node;
	reiser4_plug_t *plug;
	aal_device_t *device;

	aal_assert("umka-1268", tree != NULL);
    
	/* Allocating memory for instance of node */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	/* Finding the node plugin by its id */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid))) {
		aal_exception_error("Can't find node plugin by its id "
				    "0x%x.", pid);
		goto error_free_node;
	}

	/* Getting tree tree device and blksize in use to use them for creating
	   new node. */
	size = reiser4_tree_get_blksize(tree);
	device = reiser4_tree_get_device(tree);

	/* Allocate new node of @size at @nr. */
	if (!(block = aal_block_alloc(device, size, nr)))
		goto error_free_node;

	/* Requesting the plugin for initialization node entity. */
	if (!(node->entity = plug_call(plug->o.node_ops, init,
				       block, tree->key.plug)))
	{
		goto error_free_block;
	}

	reiser4_place_assign(&node->p, NULL, 0, MAX_UINT32);

	/* Making node header. */
	if (reiser4_node_fresh(node, level))
		goto error_free_entity;
	
	return node;

 error_free_entity:
	plug_call(plug->o.node_ops, fini,
		  node->entity);
 error_free_block:
	aal_block_free(block);
 error_free_node:    
	aal_free(node);
	return NULL;
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
	return node->counter > 0 ? TRUE : FALSE;
}

#ifndef ENABLE_STAND_ALONE
/* Assigns @nr block number to @node. */
void reiser4_node_move(reiser4_node_t *node,
		       blk_t nr)
{
	aal_assert("umka-2248", node != NULL);
	
	plug_call(node->entity->plug->o.node_ops,
		  move, node->entity, nr);
}

/* Call node plugin fresh() method, which creates new node header with zero
   items in it, etc. */
errno_t reiser4_node_fresh(reiser4_node_t *node,
			  uint8_t level)
{
	aal_assert("umka-2052", node != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 fresh, node->entity, level);
}

/* Print passed @node to the specified @stream */
errno_t reiser4_node_print(
	reiser4_node_t *node,   /* node to be printed */
	aal_stream_t *stream)   /* stream for printing in */
{
	aal_assert("umka-1537", node != NULL);
	aal_assert("umka-1538", stream != NULL);
	
	return plug_call(node->entity->plug->o.node_ops,
			 print, node->entity, stream, -1, -1, 0);
}
#endif

/* Opens node on specified @tree and block number @nr. */
reiser4_node_t *reiser4_node_open(reiser4_tree_t *tree,
				  blk_t nr)
{
	uint16_t pid;
	uint32_t size;
	aal_block_t *block;
	aal_device_t *device;
        reiser4_node_t *node;
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
errno_t reiser4_node_fini(reiser4_node_t *node) {
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
errno_t reiser4_node_close(reiser4_node_t *node) {
	aal_assert("umka-824", node != NULL);
	aal_assert("umka-2286", node->counter == 0);

	plug_call(node->entity->plug->o.node_ops,
		  fini, node->entity);
	    
	aal_free(node);
	return 0;
}

/* Getting the left delimiting key. */
errno_t reiser4_node_lkey(
	reiser4_node_t *node,	         /* node for working with */
	reiser4_key_t *key)	         /* key will be stored here */
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
static int reiser4_node_ack(reiser4_node_t *node,
			    reiser4_place_t *place)
{
	ptr_hint_t ptr;
	trans_hint_t hint;
	
	if (!(place->pos.item < reiser4_node_items(place->node)))
		return 0;
	       
	if (reiser4_place_fetch(place))
		return 0;

	if (!reiser4_item_branch(place->plug))
		return 0;

	hint.count = 1;
	hint.specific = &ptr;
	
	if (plug_call(place->plug->o.item_ops, fetch,
		      (place_t *)place, &hint) != 1)
	{
		return 0;
	}

	return ptr.start == node_blocknr(node);
}
#endif

/* Makes search of nodeptr position in parent node by passed child node. This is
   used for updating parent position in nodes. */
errno_t reiser4_node_realize(
	reiser4_node_t *node)	        /* node position will be obtained for */
{
        reiser4_key_t lkey;
	reiser4_place_t *parent;
    
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

        reiser4_node_lkey(node, &lkey);
                                                                                                   
	/* Getting position by means of using node lookup */
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
		lookup_t res;
		uint32_t i, j;
		ptr_hint_t ptr;
		trans_hint_t hint;
		
		for (i = 0; i < reiser4_node_items(parent->node); i++) {
			parent->pos.item = i;

			if ((res = reiser4_place_fetch(parent)))
				return res;

			if (!reiser4_item_branch(parent->plug))
				continue;

			for (j = 0; j < reiser4_item_units(parent); j++) {
				parent->pos.unit = j;

				hint.count = 1;
				hint.specific = &ptr;
				
				if (plug_call(parent->plug->o.item_ops, fetch,
					      (place_t *)parent, &hint) != 1)
				{
					return -EIO;
				}

				if (ptr.start == node_blocknr(node))
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

/* Helper function for walking though children list in order to find convenient
   one (block number is the same as pased @blk) */
static int callback_comp_blk(
	const void *node,		/* node find will operate on */
	const void *blk,		/* block number to be found */
	void *data)			/* user-specified data */
{
	if (*(blk_t *)blk < node_blocknr((reiser4_node_t *)node))
		return -1;

	if (*(blk_t *)blk > node_blocknr((reiser4_node_t *)node))
		return 1;

	return 0;
}

/* Finds child node by block number. */
reiser4_node_t *reiser4_node_child(
	reiser4_node_t *node,	        /* node to be greped */
	blk_t blk)                      /* block number to be found */
{
	aal_list_t *list;
    
	if (!node->children)
		return NULL;
    
	/* Using aal_list_find_custom function with local helper function for
	   comparing block numbers. */
	if ((list = aal_list_find_custom(node->children, (void *)&blk,
					 callback_comp_blk, NULL)))
	{
		return (reiser4_node_t *)list->data;
	}

	return NULL;
}

/* Helper callback function for comparing two nodes during registering of new
   child. */
static int callback_comp_node(
	const void *node1,              /* the first node for comparing */
	const void *node2,              /* the second node for comparing */
	void *data)		        /* user-specified data */
{
	reiser4_key_t lkey1;
	reiser4_key_t lkey2;

	reiser4_node_lkey((reiser4_node_t *)node1,
			  &lkey1);
	
	reiser4_node_lkey((reiser4_node_t *)node2,
			  &lkey2);
	
	return reiser4_key_compfull(&lkey1, &lkey2);
}

/* Addes passed @child into children list of @node */
errno_t reiser4_node_connect(reiser4_node_t *node,
			     reiser4_node_t *child)
{
	errno_t res;
	aal_list_t *curr;

	aal_assert("umka-1758", node != NULL);
	aal_assert("umka-1759", child != NULL);

	/* Insert @child into @node->children list. */
	curr = aal_list_insert_sorted(node->children, child,
				      callback_comp_node, NULL);

	/* Assign @node to @child parent pointer. */
	child->p.node = node;
	reiser4_node_lock(node);
	
	/* Updating node pos in parent node */
	if ((res = reiser4_node_realize(child))) {
		aal_exception_error("Can't realize node %llu.",
				    node_blocknr(child));
		return res;
	}

	if (!curr->prev)
		node->children = curr;

	return 0;
}

/* Removes passed @child from children list of @node */
errno_t reiser4_node_disconnect(
	reiser4_node_t *node,	        /* node child will be detached from */
	reiser4_node_t *child)	        /* pointer to child to be deleted */
{
	aal_list_t *next;

	aal_assert("umka-2321", node != NULL);
	
	if (!node->children)
		return -EINVAL;
    
	child->p.node = NULL;
    
	/* Updating node children list */
	next = aal_list_remove(node->children, child);
	
	if (!next || !next->prev)
		node->children = next;

	reiser4_node_unlock(node);
	return 0;
}

/* This function makes search inside of specified node for passed key. Position
   will be stored in passed @pos. */
lookup_t reiser4_node_lookup(
	reiser4_node_t *node,	/* node to be grepped */
	reiser4_key_t *key,	/* key to be find */
	bias_t bias,            /* position correcting mode (insert or read) */
	pos_t *pos)	        /* found pos will be stored here */
{
	lookup_t res;
	reiser4_key_t maxkey;
	reiser4_place_t place;
    
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
		if (place.plug->o.item_ops->maxposs_key) {
			reiser4_item_maxposs_key(&place, &maxkey);

			if (reiser4_key_compfull(key, &maxkey) > 0) {
				pos->item++;
				return ABSENT;
			}
		}
	
		/* Calling lookup method of found item */
		if (place.plug->o.item_ops->lookup) {
			reiser4_place_t *p = &place;
				
			res = plug_call(place.plug->o.item_ops, lookup,
					(place_t *)p, key, bias);
			
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
uint32_t reiser4_node_items(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 items, node->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL);

	return plug_call(node->entity->plug->o.node_ops, 
			 overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 maxspace, node->entity);
}

/* Expands passed @node at @pos by @len */
errno_t reiser4_node_expand(reiser4_node_t *node, pos_t *pos,
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
errno_t reiser4_node_shrink(reiser4_node_t *node, pos_t *pos,
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
errno_t reiser4_node_shift(
	reiser4_node_t *node,
	reiser4_node_t *neig,
	shift_hint_t *hint)
{
	errno_t res;
	uint32_t i, items;
	reiser4_plug_t *plug;
    
	aal_assert("umka-1225", node != NULL);
	aal_assert("umka-1226", neig != NULL);
	aal_assert("umka-1227", hint != NULL);

	/* Trying shift something from @node into @neig. As result insert point
	   may be shifted too. */
	plug = node->entity->plug;
	
	if ((res = plug_call(plug->o.node_ops, shift,
			     node->entity, neig->entity, hint)))
	{
		return res;
	}

	/* We do not need update children if @node does not have children at all
	   or shift did not shift any items and units. */
	if (hint->items == 0 && hint->units == 0)
		return 0;

	if (!node->children)
		return 0;

	/* Updating children lists. We have to move child node from @node to
	   @neig. */
	items = reiser4_node_items(neig);
	
	for (i = 0; i < hint->items; i++) {
		uint32_t units;
		reiser4_place_t place;

		/* Initializing the place we will start from. */
		if (hint->control & MSF_LEFT) {
			reiser4_place_assign(&place, neig,
					     items - i - 1,
					     MAX_UINT32);
		} else {
			reiser4_place_assign(&place, neig,
					     i, MAX_UINT32);
		}

		if ((res = reiser4_place_fetch(&place)))
			return res;

		/* Check if we deal with nodeptr at all, because we are not
		   interested in extent yet. */
		if (!reiser4_item_branch(place.plug))
			continue;

		place.pos.unit = 0;
		units = reiser4_item_units(&place);
		
		for (; place.pos.unit < units; place.pos.unit++) {
			ptr_hint_t ptr;
			trans_hint_t hint;
			reiser4_node_t *child;

			/* Getting nodeptr and looking for the cached child by
			   using it. */
			hint.count = 1;
			hint.specific = &ptr;

			if (plug_call(place.plug->o.item_ops, fetch,
				      (place_t *)&place, &hint) < 0)
			{
				return -EIO;
			}

			/* Getting child node by nodeptr coord from parent node
			   loaded children list. If it is there, by passing this
			   pos. */
			if (!(child = reiser4_node_child(node, ptr.start)))
			        continue;

			/* Disconnecting @child from the old parent and connect
			   it to the new one. */
			reiser4_node_disconnect(node, child);

			if ((res = reiser4_node_connect(neig, child)))
				return res;
		}

	}

	/* Updating children positions in both nodes */
	if (hint->control & MSF_LEFT) {
		pos_t pos;

		/* Updating neighbour starting from the first moved item */
		POS_INIT(&pos, reiser4_node_items(neig) -
			 hint->items - 1, MAX_UINT32);

		if ((res = reiser4_node_uchild(neig, &pos)))
			return res;
		
		/* Updating @node starting from the first item */
		POS_INIT(&pos, 0, MAX_UINT32);
		
		if ((res = reiser4_node_uchild(node, &pos)))
			return res;
	} else {
		pos_t pos;

		/* Updating neighbour starting from the first item */
		POS_INIT(&pos, 0, MAX_UINT32);

		if ((res = reiser4_node_uchild(neig, &pos)))
			return res;
	}

	return 0;
}

/* Saves passed @node onto device it was opened on */
errno_t reiser4_node_sync(
	reiser4_node_t *node)	/* node to be synchronized */
{
	aal_assert("umka-2253", node != NULL);
    
	/* Synchronizing passed @node */
	if (!reiser4_node_isdirty(node)) 
		return 0;
	
	return plug_call(node->entity->plug->o.node_ops, 
			 sync, node->entity);
}

/* Updates nodeptr item in parent node */
errno_t reiser4_node_uptr(reiser4_node_t *node) {
	errno_t res;
	trans_hint_t hint;
	ptr_hint_t nodeptr_hint;

	aal_assert("umka-2263", node != NULL);

	if (!node->p.node)
		return 0;
	
	aal_memset(&hint, 0, sizeof(hint));

        /* Preparing node pointer hint to be used */
	nodeptr_hint.width = 1;
	nodeptr_hint.start = node_blocknr(node);

	hint.specific = &nodeptr_hint;

	if ((res = reiser4_place_fetch(&node->p)))
		return res;

	if (plug_call(node->p.plug->o.item_ops, update,
		      (place_t *)&node->p, &hint) != 1)
	{
		return -EIO;
	}

	return 0;
}

/* Updates node keys in recursive maner (needed for updating ldkeys on the all
   levels of tre tree). */
errno_t reiser4_node_ukey(reiser4_node_t *node,
			  pos_t *pos, reiser4_key_t *key)
{
	aal_assert("umka-999", node != NULL);
	aal_assert("umka-1000", pos != NULL);
	aal_assert("umka-1001", key != NULL);

	return plug_call(node->entity->plug->o.node_ops,
			 set_key, node->entity, pos, key);
}

/* Updates children in-parent position. It is used durring internal nodes
   modifying. */
errno_t reiser4_node_uchild(reiser4_node_t *node,
			    pos_t *start)
{
	errno_t res;
	uint32_t items;

	reiser4_place_t place;
	aal_list_t *walk, *list = NULL;

	aal_assert("umka-1887", node != NULL);
	aal_assert("umka-1888", start != NULL);
	
	if (node->children == NULL)
		return 0;

	if (reiser4_node_items(node) == 0)
		return 0;

	items = reiser4_node_items(node);
	
	reiser4_place_assign(&place, node,
			     start->item, 0);

	/* Searching for first nodeptr item */
	for (; place.pos.item < items; place.pos.item++) {
		
		if ((res = reiser4_place_fetch(&place)))
			return res;

		if (reiser4_item_branch(place.plug))
			break;
	}

	if (place.pos.item < items) {
		if (!reiser4_item_branch(place.plug))
			return 0;
	}

	/* Searching for the first loaded child found nodeptr item points to */
	for (; place.pos.item < items; place.pos.item++) {
		ptr_hint_t ptr;
		trans_hint_t hint;
		reiser4_place_t *p;
		
		if ((res = reiser4_place_fetch(&place)))
			return res;
		
		if (!reiser4_item_branch(place.plug))
			continue;

		p = &place;
		hint.count = 1;
		hint.specific = &ptr;
		
		if (plug_call(place.plug->o.item_ops, fetch,
			      (place_t *)p, &hint) < 0)
		{
			return -EIO;
		}
	
		if ((list = aal_list_find_custom(node->children,
						 (void *)&ptr.start,
						 callback_comp_blk, NULL)))
		{
			break;
		}
	}

	if (list == NULL)
		return 0;

	/* Updating childrens in-parent position */
	aal_list_foreach_forward(list, walk) {
		reiser4_node_t *child = (reiser4_node_t *)walk->data;

		aal_assert("umka-1886", child->p.node == node);

		if ((res = reiser4_node_realize(child)))
			return res;
	}
	
	return 0;
}

/* Node modifying fucntion. */
int64_t reiser4_node_mod(
	reiser4_node_t *node,	         /* node item will be inserted in */
	pos_t *pos,                      /* pos item will be inserted at */
	trans_hint_t *hint,              /* item hint to be inserted */
	bool_t insert)                   /* modifying mode (insert/write) */
{
	errno_t res;
	uint32_t len;
	uint32_t needed;
	int32_t write = 0;
    
	len = hint->len + hint->ohd;

	needed = len + (pos->unit == MAX_UINT32 ?
			reiser4_node_overhead(node) : 0);
	
	/* Checking if item length is greater then free space in the node */
	if (needed > reiser4_node_space(node)) {
		aal_exception_error("There is no space to insert new "
				    "item/unit of (%u) size in the node "
				    "(%llu).", len, node_blocknr(node));
		return -EINVAL;
	}

	if (insert) {
		/* Inserting new item or pasting unit into one existent item pointed by
		   pos->item. */
		if ((res = plug_call(node->entity->plug->o.node_ops,
				     insert, node->entity, pos, hint)) < 0)
		{
			return res;
		}
	} else {
		/* Writing data to node. */
		if ((write = plug_call(node->entity->plug->o.node_ops,
				       write, node->entity, pos, hint)) < 0)
		{
			return write;
		}
	}

	if ((res = reiser4_node_uchild(node, pos))) {
		aal_exception_error("Can't update child positions in "
				    "node %llu.", node_blocknr(node));
		return res;
	}
	
	return write;
}

errno_t reiser4_node_insert(reiser4_node_t *node,
			    pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-990", node != NULL);
	aal_assert("umka-991", pos != NULL);
	aal_assert("umka-992", hint != NULL);

	return reiser4_node_mod(node, pos, hint, 1);
}

int64_t reiser4_node_write(reiser4_node_t *node,
			   pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2445", node != NULL);
	aal_assert("umka-2446", pos != NULL);
	aal_assert("umka-2447", hint != NULL);

	return reiser4_node_mod(node, pos, hint, 0);
}

int64_t reiser4_node_trunc(reiser4_node_t *node,
			   pos_t *pos, trans_hint_t *hint)
{
	aal_assert("umka-2503", node != NULL);
	aal_assert("umka-2504", pos != NULL);
	aal_assert("umka-2505", hint != NULL);
	
	return plug_call(node->entity->plug->o.node_ops,
			 truncate, node->entity, pos, hint);
}

/* Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key. */
errno_t reiser4_node_remove(
	reiser4_node_t *node,	          /* node item will be removed from */
	pos_t *pos,                       /* pos item will be removed at */
	trans_hint_t *hint)
{
	errno_t res;
	
	aal_assert("umka-993", node != NULL);
	aal_assert("umka-994", pos != NULL);
	aal_assert("umka-2391", hint != NULL);

	/* Removing item or unit. We assume that we are going to remove unit if
	   unit component is set up. */
	if ((res = plug_call(node->entity->plug->o.node_ops,
			     remove, node->entity, pos, hint)))
	{
		aal_exception_error("Can't remove %llu items/units "
				    "from node %llu.", hint->count,
				    node_blocknr(node));
		return res;
	}

	if (reiser4_node_items(node) > 0) {
		/* Updating children */
		if ((res = reiser4_node_uchild(node, pos)))
			return res;
	}
	
	return 0;
}

void reiser4_node_set_mstamp(reiser4_node_t *node, uint32_t stamp) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-646", node != NULL);
	aal_assert("umka-1969", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->set_mstamp)
		plug->o.node_ops->set_mstamp(node->entity, stamp);
}

void reiser4_node_set_fstamp(reiser4_node_t *node, uint64_t stamp) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-648", node != NULL);
	aal_assert("umka-1970", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->get_fstamp)
		plug->o.node_ops->set_fstamp(node->entity, stamp);
}

void reiser4_node_set_level(reiser4_node_t *node,
			    uint8_t level)
{
	aal_assert("umka-1863", node != NULL);
    
	plug_call(node->entity->plug->o.node_ops, 
		  set_level, node->entity, level);
}

uint32_t reiser4_node_get_mstamp(reiser4_node_t *node) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-562", node != NULL);
	aal_assert("umka-1971", node->entity != NULL);

	plug = node->entity->plug;
	
	if (plug->o.node_ops->get_mstamp)
		return plug->o.node_ops->get_mstamp(node->entity);
	
	return 0;
}

uint64_t reiser4_node_get_fstamp(reiser4_node_t *node) {
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
uint8_t reiser4_node_get_level(reiser4_node_t *node) {
	aal_assert("umka-1642", node != NULL);
    
	return plug_call(node->entity->plug->o.node_ops, 
			 get_level, node->entity);
}
