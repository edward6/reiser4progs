/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node.c -- the reiser4 disk node personalization. The libreiser4 internal
   in-memory tree consists of reiser4_node_t instances. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_node_isdirty(reiser4_node_t *node) {
	aal_assert("umka-2094", node != NULL);

	return plugin_call(node->entity->plugin->o.node_ops,
			   isdirty, node->entity);
}

void reiser4_node_mkdirty(reiser4_node_t *node) {
	aal_assert("umka-2095", node != NULL);

	plugin_call(node->entity->plugin->o.node_ops,
		    mkdirty, node->entity);
}

void reiser4_node_mkclean(reiser4_node_t *node) {
	aal_assert("umka-2096", node != NULL);

	plugin_call(node->entity->plugin->o.node_ops,
		    mkclean, node->entity);
}

errno_t reiser4_node_clone(reiser4_node_t *src,
			   reiser4_node_t *dst)
{
	aal_assert("umka-2306", src != NULL);
	aal_assert("umka-2307", dst != NULL);

	return plugin_call(src->entity->plugin->o.node_ops,
			   clone, src->entity, dst->entity);
}
#endif

reiser4_node_t *reiser4_node_init(aal_device_t *device,
				  uint32_t size, blk_t blk,
				  rid_t pid)
{
	reiser4_node_t *node;
	reiser4_plugin_t *plugin;

	aal_assert("umka-1268", device != NULL);
    
	/* Allocating memory for instance of node */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	/* Finding the node plugin by its id */
	if (!(plugin = libreiser4_factory_ifind(NODE_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find node plugin by its id "
				    "0x%x.", pid);
		goto error_free_node;
	}

	/* Requesting the plugin for initialization of the entity */
	if (!(node->entity = plugin_call(plugin->o.node_ops, init,
					 device, size, blk)))
	{
		goto error_free_node;
	}

	node->size = size;
	node->number = blk;
	node->device = device;

	reiser4_place_assign(&node->p, NULL, 0, ~0ul);
	return node;

 error_free_node:    
	aal_free(node);
	return NULL;
}
	
errno_t reiser4_node_load(reiser4_node_t *node) {
	aal_assert("umka-2053", node != NULL);

	return plugin_call(node->entity->plugin->o.node_ops,
			   load, node->entity);
}

errno_t reiser4_node_unload(reiser4_node_t *node) {
	aal_assert("umka-2054", node != NULL);

#ifndef ENABLE_STAND_ALONE
	if (reiser4_node_isdirty(node))
		reiser4_node_sync(node);
#endif
	
	return plugin_call(node->entity->plugin->o.node_ops,
			   unload, node->entity);
}

void reiser4_node_lock(reiser4_node_t *node) {
	aal_assert("umka-2314", node != NULL);
	node->counter++;
}

void reiser4_node_unlock(reiser4_node_t *node) {
	aal_assert("umka-2316", node != NULL);
	aal_assert("umka-2316", node->counter > 0);
	node->counter--;
}

bool_t reiser4_node_locked(reiser4_node_t *node) {
	return node->counter > 0 ? TRUE : FALSE;
}

#ifndef ENABLE_STAND_ALONE
void reiser4_node_move(reiser4_node_t *node,
		       blk_t number)
{
	aal_assert("umka-2248", node != NULL);

	node->number = number;
	
	plugin_call(node->entity->plugin->o.node_ops,
		    move, node->entity, number);

	reiser4_node_mkdirty(node);
}

errno_t reiser4_node_form(reiser4_node_t *node,
			  uint8_t level)
{
	aal_assert("umka-2052", node != NULL);

	return plugin_call(node->entity->plugin->o.node_ops,
			   form, node->entity, level);
}

/* Prints passed @node to the specified @stream */
errno_t reiser4_node_print(
	reiser4_node_t *node,   /* node to be printed */
	aal_stream_t *stream)   /* stream for printing in */
{
	aal_assert("umka-1537", node != NULL);
	aal_assert("umka-1538", stream != NULL);
	
	return plugin_call(node->entity->plugin->o.node_ops,
			   print, node->entity, stream, -1, -1, 0);
}
#endif

/* Helper callback for checking if passed @plugin convenient one for passed @blk
   to open it or not. */
static bool_t callback_guess_node(reiser4_plugin_t *plugin,
				  void *data)
{
	reiser4_node_t *node;

	/* We are interested only in node plugins here */
	if (plugin->h.type != NODE_PLUGIN_TYPE)
		return FALSE;
	
	node = (reiser4_node_t *)data;
	
	/* Requesting block supposed to be a correct node to be opened and
	   confirmed about its format. */
	if (!(node->entity = plugin_call(plugin->o.node_ops, init,
					 node->device, node->size,
					 node->number)))
		return FALSE;

	if (plugin_call(plugin->o.node_ops, load, node->entity))
		goto error_free_entity;
		
	/* Okay, we have found needed node plugin, now we should confirm that
	   @node is realy formatted node and it uses @plugin. */
	if (plugin_call(plugin->o.node_ops, confirm, node->entity))
		return TRUE;

 error_free_entity:
	plugin_call(plugin->o.node_ops, close,
		    node->entity);
	
	node->entity = NULL;
	return FALSE;
}

/* This function is trying to detect node plugin */
static errno_t reiser4_node_guess(reiser4_node_t *node) {

	/* Finding node plugin by its id */
	if (!libreiser4_factory_cfind(callback_guess_node, node, FALSE))
		return -EINVAL;

	return 0;
}

/* Opens node on specified device and block number */
reiser4_node_t *reiser4_node_open(aal_device_t *device,
				  uint32_t size,
				  blk_t blk)
{
        reiser4_node_t *node;
 
        aal_assert("umka-160", device != NULL);
    
        if (!(node = aal_calloc(sizeof(*node), 0)))
                return NULL;
 
	node->size = size;
        node->number = blk;
        node->device = device;

        if (reiser4_node_guess(node))
                goto error_free_node;
     
        reiser4_place_assign(&node->p, NULL, 0, ~0ul);
        return node;
     
 error_free_node:
        aal_free(node);
        return NULL;
}

/* Closes specified node and its children. Before the closing, this function
   also detaches nodes from the tree if they were attached. */
errno_t reiser4_node_close(reiser4_node_t *node) {
	aal_assert("umka-824", node != NULL);
	aal_assert("umka-2286", node->counter == 0);

	reiser4_node_unload(node);
	
	plugin_call(node->entity->plugin->o.node_ops,
		    close, node->entity);
	    
	aal_free(node);
	return 0;
}

/* Getting the left delimiting key */
errno_t reiser4_node_lkey(
	reiser4_node_t *node,	         /* node for working with */
	reiser4_key_t *key)	         /* key will be stored here */
{
	errno_t res;
	reiser4_place_t place;
	pos_t pos = {0, ~0ul};

	aal_assert("umka-753", node != NULL);
	aal_assert("umka-754", key != NULL);

	if ((res = reiser4_place_open(&place, node, &pos)))
		return res;

	return reiser4_key_assign(key, &place.item.key);
}

#ifndef ENABLE_STAND_ALONE
/* Acknowledles, that passed @place has nodeptr that points onto passed
   @node. This is needed for node_realize() function. */
static int reiser4_node_ack(reiser4_node_t *node,
			    reiser4_place_t *place)
{
	ptr_hint_t ptr;
	
	if (!(place->pos.item < reiser4_node_items(place->node)))
		return 0;
	       
	if (reiser4_place_realize(place))
		return 0;

	if (!reiser4_item_branch(place))
		return 0;

	plugin_call(place->item.plugin->o.item_ops, read,
		    &place->item, &ptr, place->pos.unit, 1);

	return ptr.start == node->number;
}
#endif

/* Makes search of nodeptr position in parent node by passed child node. This is
   used for updating parent position in nodes. */
errno_t reiser4_node_realize(
	reiser4_node_t *node)	        /* node, position will be obtained
					   for */
{
        lookup_t res;
	uint32_t i, j;
	ptr_hint_t ptr;

        reiser4_key_t lkey;
	reiser4_place_t *parent;
    
	aal_assert("umka-869", node != NULL);
	aal_assert("umka-1941", node->p.node != NULL);

	parent = &node->p;

	/* Checking if we are in position already */
#ifndef ENABLE_STAND_ALONE
	if (!(node->flags & NF_FOREIGN)) {
		if (reiser4_node_ack(node, parent))
			goto parent_realize;
	}
#endif
	
	/* Getting position by means of using node lookup */
        reiser4_node_lkey(node, &lkey);
                                                                                                   
        if (reiser4_node_lookup(parent->node, &lkey,
				&parent->pos) == PRESENT)
	{
#ifndef ENABLE_STAND_ALONE
		if (!(node->flags & NF_FOREIGN)) {
			if (reiser4_node_ack(node, parent))
				goto parent_realize;
		} else
#endif
			goto parent_realize;
	}

	/* Getting position by means of linear traverse */
#ifndef ENABLE_STAND_ALONE
	if (!(node->flags & NF_FOREIGN)) {
		for (i = 0; i < reiser4_node_items(parent->node); i++) {
			parent->pos.item = i;

			if ((res = reiser4_place_realize(parent)))
				return res;

			if (!reiser4_item_branch(parent))
				continue;

			for (j = 0; j < reiser4_item_units(parent); j++) {
				parent->pos.unit = j;
			
				plugin_call(parent->item.plugin->o.item_ops,
					    read, &parent->item, &ptr, j, 1);

				if (ptr.start == node->number)
					goto parent_realize;
			}
		}
	}

	return -EINVAL;
#endif
	
 parent_realize:
	if (reiser4_place_realize(parent))
		return -EINVAL;

	if (reiser4_item_units(parent) == 1)
		parent->pos.unit = ~0ul;

	return 0;
}

/* Helper function for walking though children list in order to find convenient
 * one (block number is the same as pased @blk) */
static int callback_comp_blk(
	const void *node,		/* node find will operate on */
	const void *blk,		/* key to be find */
	void *data)			/* user-specified data */
{
	if (*(blk_t *)blk < ((reiser4_node_t *)node)->number)
		return -1;

	if (*(blk_t *)blk > ((reiser4_node_t *)node)->number)
		return 1;

	return 0;
}

/* Finds child node by block number */
reiser4_node_t *reiser4_node_child(
	reiser4_node_t *node,	        /* node to be greped */
	blk_t blk)                      /* left delimiting key */
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
	const void *node1,              /* the first node instance for comparing */
	const void *node2,              /* the second node */
	void *data)		        /* user-specified data */
{
	reiser4_key_t lkey1, lkey2;

	reiser4_node_lkey((reiser4_node_t *)node1, &lkey1);
	reiser4_node_lkey((reiser4_node_t *)node2, &lkey2);
    
	return reiser4_key_compare(&lkey1, &lkey2);
}

/* Addes passed @child into children list of @node */
errno_t reiser4_node_connect(reiser4_node_t *node,
			     reiser4_node_t *child)
{
	errno_t res;
	aal_list_t *current;

	aal_assert("umka-1758", node != NULL);
	aal_assert("umka-1759", child != NULL);
	
	current = aal_list_insert_sorted(node->children, child,
					 callback_comp_node, NULL);
	
	child->p.node = node;
	reiser4_node_lock(node);
	
	/* Updating node pos in parent node */
	if ((res = reiser4_node_realize(child))) {
		aal_exception_error("Can't realize node %llu.",
				    child->number);
		return res;
	}

	if (!current->prev)
		node->children = current;

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

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_node_confirm(reiser4_node_t *node) {
	aal_assert("umka-123", node != NULL);
    
	return plugin_call(node->entity->plugin->o.node_ops, 
			   confirm, node->entity);
}
#endif

/* This function makes search inside of specified node for passed key. Position
   will be stored in passed @pos. */
lookup_t reiser4_node_lookup(
	reiser4_node_t *node,	/* node to be grepped */
	reiser4_key_t *key,	/* key to be find */
	pos_t *pos)	        /* found pos will be stored here */
{
	lookup_t res;
	item_entity_t *item;
	reiser4_key_t maxkey;
	reiser4_place_t place;
    
	aal_assert("umka-475", pos != NULL);
	aal_assert("vpf-048", node != NULL);
	aal_assert("umka-476", key != NULL);

	POS_INIT(pos, 0, ~0ul);

	/* Calling node plugin lookup method */
	if ((res = plugin_call(node->entity->plugin->o.node_ops, lookup,
			       node->entity, key, pos)) == FAILED)
	{
		return FAILED;
	}

	if (res == ABSENT) {
		/* Check if node was empty */
		if (pos->item == 0)
			return ABSENT;
		
		/* Correcting position, as node plugin lookup returns position
		   next to convenient item. */
		pos->item--;
	}
		
	if (reiser4_place_open(&place, node, pos))
		return FAILED;
		
	item = &place.item;

	if (res == ABSENT) {
		/* We are on the position where key is less then wanted. Key
		   could lie within the item or after the item. */
		if (item->plugin->o.item_ops->maxposs_key) {
			reiser4_item_maxposs_key(&place, &maxkey);

			if (reiser4_key_compare(key, &maxkey) > 0) {
				pos->item++;
				return ABSENT;
			}
		}
	
		/* Calling lookup method of found item */
		if (item->plugin->o.item_ops->lookup) {
			return plugin_call(item->plugin->o.item_ops,
					   lookup, item, key, &pos->unit);
		}
	
		/* Lookup isn't implemented whereas maxposs_key() isn't
		   implemented too. Is it correct? */
		pos->item++;
		return ABSENT;
	} else {
		/* If item is found by its key, that means, that we can set unit
		   component to 0. This is neede to avoid creating mergeable
		   item (for instance tails) in the same node. */
		pos->unit = 0;
		return PRESENT;
	}

	return ABSENT;
}

/* Returns real item count in specified node */
uint32_t reiser4_node_items(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL);
    
	return plugin_call(node->entity->plugin->o.node_ops, 
			   items, node->entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL);
    
	return plugin_call(node->entity->plugin->o.node_ops, 
			   space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL);

	return plugin_call(node->entity->plugin->o.node_ops, 
			   overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL);
    
	return plugin_call(node->entity->plugin->o.node_ops, 
			   maxspace, node->entity);
}

/* Expands passed @node at @pos by @len */
errno_t reiser4_node_expand(reiser4_node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1815", node != NULL);
	aal_assert("umka-1816", pos != NULL);

	res = plugin_call(node->entity->plugin->o.node_ops,
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

	res = plugin_call(node->entity->plugin->o.node_ops,
			  shrink, node->entity, pos, len, count);

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
	reiser4_plugin_t *plugin;
    
	aal_assert("umka-1225", node != NULL);
	aal_assert("umka-1226", neig != NULL);
	aal_assert("umka-1227", hint != NULL);

	/* Trying shift something from @node into @neig. As result insert point
	   may be shifted too. */
	plugin = node->entity->plugin;
	
	if ((res = plugin_call(plugin->o.node_ops, shift,
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

	/* Updating children lists in node and its neighbour */
	items = reiser4_node_items(neig);
	
	for (i = 0; i < hint->items; i++) {
		uint32_t units;
		reiser4_place_t place;

		reiser4_place_assign(&place, neig,
				     (hint->control & SF_LEFT) ?
				     items - i - 1 : i, ~0ul);

		if ((res = reiser4_place_realize(&place)))
			return res;

		if (!reiser4_item_branch(&place))
			continue;

		place.pos.unit = 0;
		units = reiser4_item_units(&place);
		
		for (; place.pos.unit < units; place.pos.unit++) {
			ptr_hint_t ptr;
			reiser4_node_t *child;

			/* Getting nodeptr and looking for the cached child by
			   using it. */
			plugin_call(place.item.plugin->o.item_ops, read,
				    &place.item, &ptr, place.pos.unit, 1);
			
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
	if (hint->control & SF_LEFT) {
		pos_t pos;

		/* Updating neighbour starting from the first moved item */
		POS_INIT(&pos, reiser4_node_items(neig) -
			 hint->items - 1, ~0ul);

		if ((res = reiser4_node_uchildren(neig, &pos)))
			return res;
		
		/* Updating @node starting from the first item */
		POS_INIT(&pos, 0, ~0ul);
		
		if ((res = reiser4_node_uchildren(node, &pos)))
			return res;
	} else {
		pos_t pos;

		/* Updating neighbour starting from the first item */
		POS_INIT(&pos, 0, ~0ul);

		if ((res = reiser4_node_uchildren(neig, &pos)))
			return res;
	}

	return 0;
}

/* Saves passed @node onto device it was opened on */
errno_t reiser4_node_sync(
	reiser4_node_t *node)	/* node to be synchronized */
{
	errno_t res;
	
	aal_assert("umka-2253", node != NULL);
    
	/* Synchronizing passed @node */
	if (reiser4_node_isdirty(node)) {

		if ((res = plugin_call(node->entity->plugin->o.node_ops,
				       sync, node->entity)))
		{
			aal_exception_error("Can't synchronize node %llu "
					    "to device. %s.", node->number,
					    node->device->error);

			return res;
		}
	}

	return 0;
}

/* Updates nodeptr item in parent node */
errno_t reiser4_node_update(reiser4_node_t *node) {
	errno_t res;
	create_hint_t hint;
	
	reiser4_place_t *place;
	ptr_hint_t nodeptr_hint;

	aal_assert("umka-2263", node != NULL);

	place = &node->p;

	if (!place->node)
		return 0;
	
	aal_memset(&hint, 0, sizeof(hint));

        /* Preparing node pointer hint to be used */
	nodeptr_hint.width = 1;
	nodeptr_hint.start = node->number;
	
	hint.type_specific = &nodeptr_hint;
				
	if ((res = reiser4_place_realize(place)))
		return res;

	return reiser4_item_insert(place, &hint);
}

/* Updates node keys in recursive maner (needed for updating ldkeys on the all
   levels of tre tree). */
errno_t reiser4_node_ukey(reiser4_node_t *node,
			  pos_t *pos,
			  reiser4_key_t *key)
{
	errno_t res;
	reiser4_place_t place;
    
	aal_assert("umka-999", node != NULL);
	aal_assert("umka-1000", pos != NULL);
	aal_assert("umka-1001", key != NULL);

	if ((res = reiser4_place_open(&place, node, pos)))
		return res;

	if ((res = reiser4_item_set_key(&place, key)))
		return res;
    
	return 0;
}

/* Updates children in-parent position. It is used during internal nodes
   modifying. */
errno_t reiser4_node_uchildren(reiser4_node_t *node,
			       pos_t *start)
{
	errno_t res;
	ptr_hint_t ptr;
	uint32_t items;

	reiser4_place_t place;
	aal_list_t *walk = NULL;
	aal_list_t *list = NULL;

	aal_assert("umka-1887", node != NULL);
	aal_assert("umka-1888", start != NULL);
	
	if (node->children == NULL)
		return 0;

	if (reiser4_node_items(node) == 0)
		return 0;

	items = reiser4_node_items(node);
	
	reiser4_place_assign(&place, node, start->item, 0);

	/* Searchilg for first nodeptr item */
	for (; place.pos.item < items; place.pos.item++) {
		
		if ((res = reiser4_place_realize(&place)))
			return res;

		if (reiser4_item_branch(&place))
			break;
	}

	if (place.pos.item < items) {
		if (!reiser4_item_branch(&place))
			return 0;
	}

	/* Searching for the first loaded child found nodeptr item points to */
	for (; place.pos.item < items; place.pos.item++) {

		if ((res = reiser4_place_realize(&place)))
			return res;
		
		if (!reiser4_item_branch(&place))
			continue;
		
		plugin_call(place.item.plugin->o.item_ops, read,
			    &place.item, &ptr, place.pos.unit, 1);
	
		if ((list = aal_list_find_custom(node->children,
						 (void *)&ptr.start,
						 callback_comp_blk, NULL)))
			break;
	}

	if (!list)
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

/* Inserts item or unit into node. Keeps track of changes of the left delimiting
   keys in all parent nodes. */
errno_t reiser4_node_insert(
	reiser4_node_t *node,	         /* node item will be inserted in */
	pos_t *pos,                      /* pos item will be inserted at */
	create_hint_t *hint)             /* item hint to be inserted */
{
	errno_t res;
	uint32_t needed;
	uint32_t maxspace;
    
	aal_assert("umka-990", node != NULL);
	aal_assert("umka-991", pos != NULL);
	aal_assert("umka-992", hint != NULL);
	aal_assert("umka-1957", hint->len > 0);
	
	aal_assert("umka-761", hint->len > 0 &&
		   hint->len < reiser4_node_maxspace(node));

	needed = hint->len + (pos->unit == ~0ul ?
			      reiser4_node_overhead(node) : 0);
	
	/* Checking if item length is greater then free space in the node */
	if (needed > reiser4_node_space(node)) {
		aal_exception_error("There is no space to insert new "
				    "item/unit of (%u) size in the node "
				    "(%llu).", hint->len, node->number);
		return -EINVAL;
	}

	/* Inserting new item or pasting unit into one existent item pointed by
	   pos->item. */
	if ((res = plugin_call(node->entity->plugin->o.node_ops,
			       insert, node->entity, pos, hint)))
		return res;

	reiser4_node_mkdirty(node);
	
	if ((res = reiser4_node_uchildren(node, pos)))
		return res;
	
	return 0;
}

/* Removes some amount of item/units */
errno_t reiser4_node_cut(
	reiser4_node_t *node,	         /* node item will be removed from */
	pos_t *start,		         /* start item will be removed at */
	pos_t *end)		         /* end item will be removed at */
{
	errno_t res;
	
	aal_assert("umka-1785", node != NULL);
	aal_assert("umka-1786", start != NULL);
	aal_assert("umka-1787", end != NULL);

	/* Calling plugin's cut method */
	if ((res = plugin_call(node->entity->plugin->o.node_ops,
			       cut, node->entity, start, end)))
	{
		aal_exception_error("Can't cut items/units from the node "
				    "%llu. Start: (%lu, %lu), end: (%lu, %lu).",
				    node->number, start->item, start->unit,
				    end->item, end->unit);
		return res;
	}

	reiser4_node_mkdirty(node);
	
	/* Updating children */
	if ((res = reiser4_node_uchildren(node, start)))
		return res;
	
	return 0;
}

/* Deletes item or unit from cached node. Keeps track of changes of the left
   delimiting key. */
errno_t reiser4_node_remove(
	reiser4_node_t *node,	          /* node item will be removed from */
	pos_t *pos,                       /* pos item will be removed at */
	uint32_t count)                   /* the number of item/units */
{
	errno_t res;
	
	aal_assert("umka-993", node != NULL);
	aal_assert("umka-994", pos != NULL);

	/* Removing item or unit. We assume that we are going to remove unit if
	   unit component is set up. */
	if ((res = plugin_call(node->entity->plugin->o.node_ops,
			       remove, node->entity, pos, count)))
	{
		aal_exception_error("Can't remove %lu items/units from "
				    "node %llu.", count, node->number);
		return res;
	}

	if (reiser4_node_items(node) > 0) {
		
		reiser4_node_mkdirty(node);
	
		/* Updating children */
		if ((res = reiser4_node_uchildren(node, pos)))
			return res;
	}
	
	return 0;
}

void reiser4_node_set_mstamp(reiser4_node_t *node, uint32_t stamp) {
	reiser4_plugin_t *plugin;
	
	aal_assert("vpf-646", node != NULL);
	aal_assert("umka-1969", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->o.node_ops->set_mstamp) {
		plugin->o.node_ops->set_mstamp(node->entity, stamp);
		reiser4_node_mkdirty(node);
	}
}

void reiser4_node_set_fstamp(reiser4_node_t *node, uint64_t stamp) {
	reiser4_plugin_t *plugin;
	
	aal_assert("vpf-648", node != NULL);
	aal_assert("umka-1970", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->o.node_ops->get_fstamp) {
		plugin->o.node_ops->set_fstamp(node->entity, stamp);
		reiser4_node_mkdirty(node);
	}
}

void reiser4_node_set_level(reiser4_node_t *node,
			    uint8_t level)
{
	aal_assert("umka-1863", node != NULL);
    
	plugin_call(node->entity->plugin->o.node_ops, 
		    set_level, node->entity, level);

	reiser4_node_mkdirty(node);
}

uint32_t reiser4_node_get_mstamp(reiser4_node_t *node) {
	reiser4_plugin_t *plugin;
	
	aal_assert("vpf-562", node != NULL);
	aal_assert("umka-1971", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->o.node_ops->get_mstamp)
		return plugin->o.node_ops->get_mstamp(node->entity);
	
	return 0;
}

uint64_t reiser4_node_get_fstamp(reiser4_node_t *node) {
	reiser4_plugin_t *plugin;

	aal_assert("vpf-647", node != NULL);
	aal_assert("umka-1972", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->o.node_ops->get_fstamp)
		plugin->o.node_ops->get_fstamp(node->entity);

	return 0;
}
#endif

/* Returns node level */
uint8_t reiser4_node_get_level(reiser4_node_t *node) {
	aal_assert("umka-1642", node != NULL);
    
	return plugin_call(node->entity->plugin->o.node_ops, 
			   get_level, node->entity);
}
