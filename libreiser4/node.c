/*
  node.c -- the personalization of the reiser4 on-disk node. The libreiser4
  internal in-memory tree consists of reiser4_node_t instances.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <reiser4/reiser4.h>

#ifndef ENABLE_ALONE

/* Creates node instance based on passed device and block number */
reiser4_node_t *reiser4_node_create(
	aal_device_t *device,	/* device new node will be created on*/
	blk_t blk,		/* block new node will be created on */
	rid_t pid,		/* node plugin id to be used */
	uint8_t level)		/* node level */
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
	if (!(node->entity = plugin_call(plugin->node_ops, create, device,
					 blk, level))) 
	{
		aal_exception_error("Can't create node entity.");
		goto error_free_node;
	}

	node->blk = blk;
	node->device = device;

	reiser4_node_mkclean(node);
	reiser4_place_assign(&node->parent, NULL, 0, ~0ul);
	
	return node;

 error_free_node:    
	aal_free(node);
	return NULL;
}

/* Prints passed @node to the specified @stream */
errno_t reiser4_node_print(
	reiser4_node_t *node,   /* node to be printed */
	aal_stream_t *stream)   /* stream for printing in */
{
	aal_assert("umka-1537", node != NULL);
	aal_assert("umka-1538", stream != NULL);
	
	return plugin_call(node->entity->plugin->node_ops,
			   print, node->entity, stream, 0);
}

#endif

/*
  Helper callback for checking if passed @plugin convenient one for passed @blk
  to open it or not.
*/
static errno_t callback_guess_node(reiser4_plugin_t *plugin,
				   void *data)
{
	reiser4_node_t *node = (reiser4_node_t *)data;

	/* We are interested only in node plugins here */
	if (plugin->h.type == NODE_PLUGIN_TYPE) {

		/*
		  Requesting block supposed to be a correct node to be opened
		  and confirmed about its format.
		*/
		if (!(node->entity = plugin_call(plugin->node_ops, open,
						 node->device, node->blk)))
			return -EINVAL;

		/* Okay, we have found needed node plugin */
		if (plugin_call(plugin->node_ops, confirm, node->entity))
			return 1;

		plugin_call(plugin->node_ops, close, node->entity);
		node->entity = NULL;
	}
	
	return 0;
}

/* This function is trying to detect node plugin */
static errno_t reiser4_node_guess(reiser4_node_t *node) {

	/* Finding node plugin by its id */
	if (!libreiser4_factory_cfind(callback_guess_node, node))
		return -EINVAL;

	return 0;
}

/* Opens node on specified device and block number */
reiser4_node_t *reiser4_node_open(
	aal_device_t *device,	         /* device node will be opened on */
	blk_t blk)		         /* block number node will be opened on */
{
	reiser4_node_t *node;

	aal_assert("umka-160", device != NULL);
   
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	node->blk = blk;
	node->device = device;

	if (reiser4_node_guess(node))
		goto error_free_node;
    
#ifndef ENABLE_ALONE
	reiser4_node_mkclean(node);
#endif

	reiser4_place_assign(&node->parent,
			     NULL, 0, ~0ul);
	
	return node;
    
 error_free_node:
	aal_free(node);
	return NULL;
}

/*
  Closes specified node and ites children. Before the closing, this function
  also detaches nodes from the tree if they were attached.
*/
errno_t reiser4_node_close(reiser4_node_t *node) {
	errno_t res;
	
	aal_assert("umka-824", node != NULL);
	aal_assert("umka-903", node->entity != NULL);

#ifndef ENABLE_ALONE
	if (reiser4_node_isdirty(node))
		reiser4_node_sync(node);
#endif
	
	res = plugin_call(node->entity->plugin->node_ops,
			  close, node->entity);
	    
	aal_free(node);

	return res;
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

	return reiser4_item_get_key(&place, key);
}

/* Returns position of passed node in parent node */
errno_t reiser4_node_pos(
	reiser4_node_t *node,	        /* node position will be obtained for */
	pos_t *pos)		        /* pointer result will be stored in */
{
	lookup_t res;
	reiser4_key_t lkey;
    
	aal_assert("umka-869", node != NULL);
	aal_assert("umka-1941", node->parent.node != NULL);

	reiser4_node_lkey(node, &lkey);

	res = reiser4_node_lookup(node->parent.node, &lkey,
				  &node->parent.pos);

	if (pos)
		*pos = node->parent.pos;
    
	return res == LP_PRESENT ? 0 : -EINVAL;
}

static inline int callback_comp_blk(
	const void *item,		/* node find will operate on */
	const void *blk,		/* key to be find */
	void *data)			/* user-specified data */
{
	reiser4_node_t *node;

	node = (reiser4_node_t *)item;

	if (*(blk_t *)blk < node->blk)
		return -1;

	if (*(blk_t *)blk > node->blk)
		return 1;

	return 0;
}

/* Finds child by block number */
reiser4_node_t *reiser4_node_cbp(
	reiser4_node_t *node,	        /* node to be greped */
	blk_t blk)                      /* left delimiting key */
{
	aal_list_t *list;
	reiser4_node_t *child;
    
	if (!node->children)
		return NULL;
    
	/*
	  Using aal_list_find_custom function with local helper functions for
	  comparing block numbers.
	*/
	if (!(list = aal_list_find_custom(node->children, (void *)&blk,
					  callback_comp_blk, NULL)))
		return NULL;

	return (reiser4_node_t *)list->data;
}

/*
  Helper callback function for comparing two nodes durring registering the new
  child.
*/
static inline int callback_comp_node(
	const void *item1,		/* the first node instance for comparing */
	const void *item2,		/* the second one */
	void *data)		        /* user-specified data */
{
	reiser4_node_t *node1;
	reiser4_node_t *node2;
	reiser4_key_t lkey1, lkey2;

	node1 = (reiser4_node_t *)item1;
	node2 = (reiser4_node_t *)item2;

	reiser4_node_lkey(node1, &lkey1);
	reiser4_node_lkey(node2, &lkey2);
    
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
	aal_assert("umka-1884", reiser4_node_items(child) > 0);
	
	current = aal_list_insert_sorted(node->children, child,
					 callback_comp_node, NULL);
	
	child->parent.node = node;
	
	/* Updating node pos in parent node */
	if ((res = reiser4_node_pos(child, &child->parent.pos))) {
		aal_exception_error("Can't find child %llu in "
				    "parent node %llu.",
				    child->blk, node->blk);
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
	
	if (!node->children)
		return -EINVAL;
    
	child->parent.node = NULL;
    
	/* Updating node children list */
	next = aal_list_remove(node->children, child);
	
	if (!next || !next->prev)
		node->children = next;

	return 0;
}

bool_t reiser4_node_confirm(reiser4_node_t *node) {
	aal_assert("umka-123", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   confirm, node->entity);
}

/* 
  This function makes search inside specified node for passed key. Position will
  eb stored in passed @pos.
*/
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

	if (reiser4_node_items(node) == 0)
		return LP_ABSENT;
   
	/* Calling node plugin */
	res = plugin_call(node->entity->plugin->node_ops,
			  lookup, node->entity, key, pos);

	if (res != LP_ABSENT)
		return res;

	/* Initializing item place points to */
	if (reiser4_place_open(&place, node, pos)) {
		aal_exception_error("Can't open item by place. Node "
				    "%llu, item %u.", node->blk, pos->item);
		return LP_FAILED;
	}

	item = &place.item;

	/*
	  We are on the position where key is less then wanted. Key could lies
	  within the item or after the item.
	*/
		
	if (reiser4_item_maxposs_key(&place, &maxkey))
		return LP_FAILED;

	if (reiser4_key_compare(key, &maxkey) > 0) {
		pos->item++;
		return LP_ABSENT;
	}
	
	/* Calling lookup method of found item (most probably direntry item) */
	if (!item->plugin->item_ops.lookup)
		return LP_ABSENT;

	return item->plugin->item_ops.lookup(item, key, &pos->unit);
}

/* Returns real item count in specified node */
uint32_t reiser4_node_items(reiser4_node_t *node) {
	aal_assert("umka-453", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   items, node->entity);
}
/* Checks node for validness */
errno_t reiser4_node_valid(
	reiser4_node_t *node)	/* node to be checked */
{
	aal_assert("umka-123", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   valid, node->entity);
}

#ifndef ENABLE_ALONE

/* Returns free space of specified node */
uint16_t reiser4_node_space(reiser4_node_t *node) {
	aal_assert("umka-455", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   space, node->entity);
}

/* Returns overhead of specified node */
uint16_t reiser4_node_overhead(reiser4_node_t *node) {
	aal_assert("vpf-066", node != NULL);

	return plugin_call(node->entity->plugin->node_ops, 
			   overhead, node->entity);
}

/* Returns item max size from in specified node */
uint16_t reiser4_node_maxspace(reiser4_node_t *node) {
	aal_assert("umka-125", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   maxspace, node->entity);
}

/* Makes copy @count items from @src_node into @dst_node */
errno_t reiser4_node_copy(reiser4_node_t *dst_node, pos_t *dst_pos,
			  reiser4_node_t *src_node, pos_t *src_pos,
			  uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1819", src_node != NULL);
	aal_assert("umka-1821", dst_node != NULL);
	aal_assert("umka-1820", src_pos != NULL);
	aal_assert("umka-1822", dst_pos != NULL);

	if (count == 0)
		return 0;

	res = plugin_call(src_node->entity->plugin->node_ops,
			  copy, src_node->entity, src_pos,
			  dst_node->entity, dst_pos, count);

	if (res == 0)
		reiser4_node_mkdirty(dst_node);

	return res;
}

/* Expands passed @node at @pos by @len */
errno_t reiser4_node_expand(reiser4_node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1815", node != NULL);
	aal_assert("umka-1816", pos != NULL);

	res = plugin_call(node->entity->plugin->node_ops,
			  expand, node->entity, pos, len, count);

	if (res == 0)
		reiser4_node_mkdirty(node);

	return res;
}

/* Shrinks passed @node at @pos by @len */
errno_t reiser4_node_shrink(reiser4_node_t *node, pos_t *pos,
			    uint32_t len, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1817", node != NULL);
	aal_assert("umka-1818", pos != NULL);

	res = plugin_call(node->entity->plugin->node_ops,
			  shrink, node->entity, pos, len, count);

	if (res == 0)
		reiser4_node_mkdirty(node);

	return res;
}

/*
  Makes shift of some amount of items and units into passed neighbour. Shift
  direction and other flags are passed by @hint. Returns operation error code.
*/
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

	/*
	  Trying shift something from @node into @neig. As result insert point
	  may be shifted to.
	*/
	plugin = node->entity->plugin;
	
	if ((res = plugin_call(plugin->node_ops, shift,
			       node->entity, neig->entity, hint)))
		return res;

	/*
	  We do not need update children if @node does not have children at all
	  or shift did not shift any items and units.
	*/
	if (hint->items == 0 && hint->units == 0)
		return 0;

	/* Marking nodes as dirty */
	reiser4_node_mkdirty(node);
	reiser4_node_mkdirty(neig);
	
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
			reiser4_node_t *child;
			reiser4_ptr_hint_t ptr;

			/*
			  Getting nodeptr and looking for the cached child by
			  using it.
			*/
			plugin_call(place.item.plugin->item_ops, read,
				    &place.item, &ptr, place.pos.unit, 1);
			
			if (!(child = reiser4_node_cbp(node, ptr.ptr)))
			        continue;

			/*
			  Disconnecting @child from the old parent and connect
			  it to the new one.
			*/
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
	aal_assert("umka-124", node != NULL);
    
	/* Synchronizing passed @node */
	if (reiser4_node_isdirty(node)) {

		if ((res = plugin_call(node->entity->plugin->node_ops,
				       sync, node->entity)))
		{
			aal_exception_error("Can't synchronize node %llu "
					    "to device. %s.", node->blk,
					    node->device->error);

			return res;
		}

		reiser4_node_mkclean(node);
	}

	return 0;
}

/*
  Updates node keys in recursive maner (needed for updating ldkeys on the all
  levels of tre tree).
*/
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
    
	reiser4_node_mkdirty(node);
	return 0;
}

/*
  Updates children in-parent position. It is used durring internal nodes
  modifying.
*/
errno_t reiser4_node_uchildren(reiser4_node_t *node,
			       pos_t *start)
{
	errno_t res;
	uint32_t items;
	reiser4_place_t place;

	reiser4_ptr_hint_t ptr;
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
		
		plugin_call(place.item.plugin->item_ops, read,
			    &place.item, &ptr, place.pos.unit, 1);
	
		if ((list = aal_list_find_custom(node->children, (void *)&ptr.ptr,
						 callback_comp_blk, NULL)))
			break;
	}

	if (!list)
		return 0;

	/* Updating childrens in-parent position */
	aal_list_foreach_forward(list, walk) {
		reiser4_node_t *child = (reiser4_node_t *)walk->data;

		aal_assert("umka-1886", child->parent.node == node);

		if ((res = reiser4_node_pos(child, &child->parent.pos)))
			return res;
	}
	
	return 0;
}

/* 
  Inserts item or unit into node. Keeps track of changes of the left delimiting
  keys in all parent nodes.
*/
errno_t reiser4_node_insert(
	reiser4_node_t *node,	         /* node item will be inserted in */
	pos_t *pos,                     /* pos item will be inserted at */
	reiser4_item_hint_t *hint)	 /* item hint to be inserted */
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
	
	/* Checking if item length is gretter then free space in node */
	if (needed > reiser4_node_space(node)) {
		aal_exception_error("There is no space to insert new "
				    "item/unit of (%u) size in the node "
				    "(%llu).", hint->len, node->blk);
		return -EINVAL;
	}

	/* 
	  Inserting new item or pasting unit into one existent item pointed by
	  pos->item.
	*/
	if ((res = plugin_call(node->entity->plugin->node_ops,
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
	if ((res = plugin_call(node->entity->plugin->node_ops,
			       cut, node->entity, start, end)))
	{
		aal_exception_error("Can't cut items/units from the node "
				    "%llu. Start: (%lu, %lu), end: (%lu, %lu).",
				    node->blk, start->item, start->unit,
				    end->item, end->unit);
		return res;
	}

	reiser4_node_mkdirty(node);
	
	/* Updating children */
	if ((res = reiser4_node_uchildren(node, start)))
		return res;
	
	return 0;
}

/* 
  Deletes item or unit from cached node. Keeps track of changes of the left
  delimiting key.
*/
errno_t reiser4_node_remove(
	reiser4_node_t *node,	            /* node item will be removed from */
	pos_t *pos,                        /* pos item will be removed at */
	uint32_t count)                     /* the number of item/units */
{
	errno_t res;
	
	aal_assert("umka-993", node != NULL);
	aal_assert("umka-994", pos != NULL);

	/*
	  Removing item or unit. We assume that we are going to remove unit if
	  unit component is set up.
	*/
	if ((res = plugin_call(node->entity->plugin->node_ops,
			       remove, node->entity, pos, count)))
	{
		aal_exception_error("Can't remove %lu items/units from "
				    "node %llu.", count, node->blk);
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
	
	if (plugin->node_ops.set_mstamp) {
		plugin->node_ops.set_mstamp(node->entity, stamp);
		reiser4_node_mkdirty(node);
	}
}

void reiser4_node_set_fstamp(reiser4_node_t *node, uint64_t stamp) {
	reiser4_plugin_t *plugin;
	
	aal_assert("vpf-648", node != NULL);
	aal_assert("umka-1970", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->node_ops.get_fstamp) {
		plugin->node_ops.set_fstamp(node->entity, stamp);
		reiser4_node_mkdirty(node);
	}
}

void reiser4_node_set_level(reiser4_node_t *node,
			    uint8_t level)
{
	aal_assert("umka-1863", node != NULL);
    
	plugin_call(node->entity->plugin->node_ops, 
		    set_level, node->entity, level);

	reiser4_node_mkdirty(node);
}

#endif

uint32_t reiser4_node_get_mstamp(reiser4_node_t *node) {
	reiser4_plugin_t *plugin;
	
	aal_assert("vpf-562", node != NULL);
	aal_assert("umka-1971", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->node_ops.get_mstamp)
		return plugin->node_ops.get_mstamp(node->entity);
	
	return 0;
}

uint64_t reiser4_node_get_fstamp(reiser4_node_t *node) {
	reiser4_plugin_t *plugin;

	aal_assert("vpf-647", node != NULL);
	aal_assert("umka-1972", node->entity != NULL);

	plugin = node->entity->plugin;
	
	if (plugin->node_ops.get_fstamp)
		plugin->node_ops.get_fstamp(node->entity);

	return 0;
}

/* Returns node level */
uint8_t reiser4_node_get_level(reiser4_node_t *node) {
	aal_assert("umka-1642", node != NULL);
    
	return plugin_call(node->entity->plugin->node_ops, 
			   get_level, node->entity);
}

